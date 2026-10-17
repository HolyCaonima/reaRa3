#include "ra3/gamelogic/AIState7A.h"

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string g_log;
std::map<std::uint32_t, std::uint32_t> g_ret;
std::uint32_t g_vret[0x300]{};

struct Tag {
    std::uint8_t* base = nullptr;
    std::uint32_t size = 0;
    const char* name = "";
};

std::vector<Tag> g_tags;

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

std::uint32_t bits(const void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::string hex(std::uint32_t value) {
    std::ostringstream text;
    text << std::hex << std::nouppercase << value;
    return text.str();
}

std::string hex8(std::uint32_t value) {
    std::ostringstream text;
    text << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << value;
    return text.str();
}

std::string fmt(std::uint32_t value, bool stack) {
    if (stack) {
        return "s";
    }
    for (const Tag& tag : g_tags) {
        const std::uint32_t base = bits(tag.base);
        if (tag.base != nullptr && value >= base && value - base < tag.size) {
            const std::uint32_t off = value - base;
            if (off == 0) {
                return tag.name;
            }
            return std::string(tag.name) + "+" + hex(off);
        }
    }
    return hex(value);
}

std::string kindOf(std::uint32_t value) {
    for (const Tag& tag : g_tags) {
        if (tag.base != nullptr && value == bits(tag.base)) {
            return tag.name;
        }
    }
    return hex(value);
}

bool stackArg(std::uint32_t address, int index) {
    if (index != 0) {
        return false;
    }
    return address == 0x0079E530 || address == 0x005AC4F0 || address == 0x006AD4B0 || address == 0x005AD150;
}

std::uint32_t __cdecl onDirect(std::uint32_t address, void* ecx, std::uint32_t a0, std::uint32_t a1,
                               std::uint32_t a2, int nargs) {
    std::ostringstream text;
    text << "d" << std::hex << std::nouppercase << address << ":" << fmt(bits(ecx), false);
    const std::uint32_t args[3] = {a0, a1, a2};
    for (int i = 0; i < nargs; ++i) {
        text << ":" << fmt(args[i], stackArg(address, i));
    }
    addLog(text.str());
    const auto found = g_ret.find(address);
    return found == g_ret.end() ? 0 : found->second;
}

void logV(int slot, const std::uint32_t* args, int nargs) {
    std::ostringstream text;
    text << "v" << std::hex << std::nouppercase << slot;
    for (int i = 0; i < nargs; ++i) {
        text << ":" << fmt(args[i], false);
    }
    addLog(text.str());
}

std::uint32_t __fastcall v00(void*, void*, std::uint32_t arg) {
    logV(0x00, &arg, 1);
    return g_vret[0x00];
}
std::uint32_t __fastcall v08(void*, void*) {
    logV(0x08, nullptr, 0);
    return g_vret[0x08];
}
std::uint32_t __fastcall v0c(void*, void*, std::uint32_t arg) {
    logV(0x0C, &arg, 1);
    return g_vret[0x0C];
}
std::uint32_t __fastcall v10(void*, void*) {
    logV(0x10, nullptr, 0);
    return g_vret[0x10];
}
std::uint32_t __fastcall v18(void*, void*, std::uint32_t arg) {
    auto* bytes = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(arg));
    std::ostringstream text;
    text << "v18:" << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
         << static_cast<int>(bytes[0]) << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[1]);
    addLog(text.str());
    return g_vret[0x18];
}
std::uint32_t __fastcall v1c(void*, void*, std::uint32_t arg) {
    logV(0x1C, &arg, 1);
    return g_vret[0x1C];
}
std::uint32_t __fastcall v20(void*, void*, std::uint32_t arg) {
    logV(0x20, &arg, 1);
    return g_vret[0x20];
}
std::uint32_t __fastcall v34(void*, void*, std::uint32_t arg) {
    logV(0x34, &arg, 1);
    return g_vret[0x34];
}
std::uint32_t __fastcall v38_0(void*, void*) {
    logV(0x38, nullptr, 0);
    return g_vret[0x38];
}
std::uint32_t __fastcall v38_1(void*, void*, std::uint32_t arg) {
    logV(0x38, &arg, 1);
    return g_vret[0x38];
}
std::uint32_t __fastcall v40(void*, void*, std::uint32_t a0, std::uint32_t a1) {
    const std::uint32_t args[2] = {a0, a1};
    logV(0x40, args, 2);
    return g_vret[0x40];
}
std::uint32_t __fastcall v4c(void*, void*) {
    logV(0x4C, nullptr, 0);
    return g_vret[0x4C];
}
std::uint32_t __fastcall v54(void*, void*) {
    logV(0x54, nullptr, 0);
    return g_vret[0x54];
}
std::uint32_t __fastcall v58(void*, void*, std::uint32_t arg) {
    logV(0x58, &arg, 1);
    return g_vret[0x58];
}
std::uint32_t __fastcall v6c(void*, void*, std::uint32_t arg) {
    logV(0x6C, &arg, 1);
    return g_vret[0x6C];
}
std::uint32_t __fastcall v70(void*, void*, std::uint32_t arg) {
    logV(0x70, &arg, 1);
    return g_vret[0x70];
}
std::uint32_t __fastcall v84_0(void*, void*) {
    logV(0x84, nullptr, 0);
    return g_vret[0x84];
}
std::uint32_t __fastcall v84_1(void*, void*, std::uint32_t arg) {
    logV(0x84, &arg, 1);
    return g_vret[0x84];
}
std::uint32_t __fastcall v13c(void*, void*) {
    logV(0x13C, nullptr, 0);
    return g_vret[0x13C];
}
std::uint32_t __fastcall v16c(void*, void*, std::uint32_t arg) {
    logV(0x16C, &arg, 1);
    return g_vret[0x16C];
}
std::uint32_t __fastcall v26c(void*, void*, std::uint32_t arg) {
    logV(0x26C, &arg, 1);
    return g_vret[0x26C];
}
std::uint32_t __fastcall v288(void*, void*) {
    logV(0x288, nullptr, 0);
    return g_vret[0x288];
}

struct Arena {
    std::vector<std::vector<std::uint8_t>> blocks;
    std::uint8_t* alloc(std::size_t size) {
        blocks.emplace_back(size, static_cast<std::uint8_t>(0));
        return blocks.back().data();
    }
};

void putFn(std::uint8_t* vt, int slot, const void* fn) {
    storeU32(vt + slot, bits(fn));
}

struct Graph {
    std::uint8_t* self = nullptr;
    std::uint8_t* mach = nullptr;
    std::uint8_t* own = nullptr;
    std::uint8_t* tmpl = nullptr;
    std::uint8_t* goal = nullptr;
    std::uint8_t* info = nullptr;
    std::uint8_t* loco = nullptr;
    std::uint8_t* mod = nullptr;
    std::uint8_t* prod = nullptr;
    std::uint8_t* wpn = nullptr;
    std::uint8_t* xfer = nullptr;
    std::uint8_t* game = nullptr;
    std::uint8_t* box = nullptr;
    std::uint8_t* phys = nullptr;
    std::uint8_t* inner = nullptr;
    std::uint8_t* host = nullptr;
    std::uint8_t* path = nullptr;
    std::uint8_t* sub = nullptr;
    std::uint8_t* block = nullptr;
};

Graph makeGraph(Arena& arena) {
    Graph g;
    auto vt = [&](std::uint8_t*& slot) {
        slot = arena.alloc(0x300);
    };
    std::uint8_t *sv, *mv, *lv, *modv, *pv, *wv, *xv, *bv, *iv, *hv, *pathv, *subv;
    vt(sv);
    vt(mv);
    vt(lv);
    vt(modv);
    vt(pv);
    vt(wv);
    vt(xv);
    vt(bv);
    vt(iv);
    vt(hv);
    vt(pathv);
    vt(subv);
    putFn(sv, 0x1C, &v1c);
    putFn(sv, 0x38, &v38_1);
    putFn(sv, 0x4C, &v4c);
    putFn(mv, 0x1C, &v1c);
    putFn(mv, 0x38, &v38_1);
    putFn(lv, 0x26C, &v26c);
    putFn(lv, 0x288, &v288);
    putFn(modv, 0x0C, &v0c);
    putFn(modv, 0x34, &v34);
    putFn(modv, 0x38, &v38_0);
    putFn(modv, 0x54, &v54);
    putFn(pv, 0x08, &v08);
    putFn(pv, 0x40, &v40);
    putFn(pv, 0x84, &v84_0);
    putFn(wv, 0x13C, &v13c);
    putFn(wv, 0x16C, &v16c);
    putFn(xv, 0x18, &v18);
    putFn(xv, 0x20, &v20);
    putFn(xv, 0x58, &v58);
    putFn(xv, 0x6C, &v6c);
    putFn(xv, 0x70, &v70);
    putFn(xv, 0x84, &v84_1);
    putFn(bv, 0x40, &v40);
    putFn(iv, 0x70, &v70);
    putFn(hv, 0x00, &v00);
    putFn(pathv, 0x10, &v10);
    putFn(subv, 0x00, &v00);
    putFn(pv, 0x00, &v00);

    auto obj = [&](std::size_t size, std::uint8_t* vtable) {
        auto* pointer = arena.alloc(size);
        if (vtable != nullptr) {
            storeU32(pointer, bits(vtable));
        }
        return pointer;
    };
    g.self = obj(0x90, sv);
    g.mach = obj(0x80, mv);
    g.own = obj(0x480, nullptr);
    g.tmpl = obj(0x100, nullptr);
    g.goal = obj(0x480, nullptr);
    g.info = obj(0x100, nullptr);
    g.loco = obj(0x440, lv);
    g.mod = obj(0x80, modv);
    g.prod = obj(0x80, sv);
    g.wpn = obj(0x40, wv);
    g.xfer = obj(0x20, xv);
    g.game = obj(0x60, nullptr);
    g.box = obj(0x20, bv);
    g.phys = obj(0x40, pv);
    g.inner = obj(0x80, iv);
    g.host = obj(0x40, hv);
    g.path = obj(0x20, pathv);
    g.sub = obj(0x20, subv);
    g.block = obj(0x80, nullptr);
    storeU32(g.self + 0x18, bits(g.mach));
    storeU32(g.mach + 0x24, bits(g.own));
    storeU32(g.own + 4, bits(g.tmpl));
    storeU32(g.own + 0x374, bits(g.loco));
    storeU32(g.goal + 4, bits(g.info));
    storeU32(g.prod + 0x68, 0x68);
    storeU32(g.mod + 0x40, 0xFFFFFFFFu);
    g.loco[0x434] = 0xAB;
    storeU32(g.loco + 0x1E8, 0x111);
    storeU32(g.loco + 0x1EC, 0x222);
    g_tags = {
        {g.self, 0x90, "self"}, {g.mach, 0x80, "mach"}, {g.own, 0x480, "own"},
        {g.tmpl, 0x100, "tmpl"}, {g.goal, 0x480, "goal"}, {g.info, 0x100, "info"},
        {g.loco, 0x440, "loco"}, {g.mod, 0x80, "mod"},   {g.prod, 0x80, "prod"},
        {g.wpn, 0x40, "wpn"},    {g.xfer, 0x20, "xfer"}, {g.game, 0x60, "game"},
        {g.box, 0x20, "box"},    {g.phys, 0x40, "phys"}, {g.inner, 0x80, "inner"},
        {g.host, 0x40, "host"},  {g.path, 0x20, "path"}, {g.sub, 0x20, "sub"},
        {g.block, 0x80, "alloc"},
    };
    return g;
}

void resetProbe() {
    g_log.clear();
    g_ret.clear();
    std::memset(g_vret, 0, sizeof(g_vret));
    g_ret[0x007985D0] = 0x55;
    g_ret[0x0079E460] = 0x77;
    g_ret[0x009CCDA0] = 0x45;
    g_ret[0x006D2020] = 0x21;
    g_ret[0x0079FF10] = 0x31;
    g_ret[0x0079EE50] = 0x32;
    g_ret[0x006A4FB0] = 0x46;
    g_ret[0x00683140] = 0x61;
    g_ret[0x006876D0] = 0x47;
    g_ret[0x006996C0] = 0x48;
    g_ret[0x0079F330] = 0x49;
    g_ret[0x007A1FC0] = 0x11;
    g_ret[0x007A44D0] = 0x12;
    g_ret[0x0054A090] = 0x18;
    g_ret[0x007578F0] = 1;
    g_vret[0x00] = 0x41;
    g_vret[0x10] = 0x21;
    g_vret[0x34] = 0x35;
    g_vret[0x40] = 0x62;
    g_vret[0x1C] = 0x33;
    g_vret[0x16C] = 0x34;
    g_vret[0x84] = 0x13;
    g_vret[0x70] = 0x14;
    g_vret[0x58] = 0x15;
    g_vret[0x6C] = 0x16;
    g_vret[0x20] = 0x17;
    g_vret[0x288] = 0x19;
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

void finish(std::uint32_t eax, std::uint8_t* self, const std::vector<int>& mem, const std::string& extra) {
    std::cout << "eax=00000000 kind=" << kindOf(eax) << " log=" << (g_log.empty() ? "." : g_log)
              << " mem=" << memList(self, mem);
    if (!extra.empty()) {
        std::cout << " " << extra;
    }
    std::cout << std::endl;
}

std::uint32_t parseU32(const std::string& token) {
    return static_cast<std::uint32_t>(std::strtoul(token.c_str(), nullptr, 0));
}

std::string ow(const Graph& g, int offset) {
    return "ow=" + hex8(loadU32(g.own + offset));
}

std::string p20(const Graph& g) {
    return std::string("p20=") + (loadU32(g.self + 0x20) == bits(g.prod) ? "1" : "0");
}

using namespace ra3::gamelogic;

}  // namespace

int main() {
    using namespace ra3::gamelogic;
    setAIState7ACalls(AIState7ACalls{onDirect, 0});
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
        std::string token;
        while (input >> token) {
            args.push_back(token);
        }
        auto arg = [&](std::size_t index) {
            return index < args.size() ? parseU32(args[index]) : 0u;
        };
        Arena arena;
        resetProbe();
        Graph g = makeGraph(arena);
        setAIState7ACalls(AIState7ACalls{onDirect, bits(g.game)});
        std::uint32_t eax = 0;
        std::vector<int> mem;
        std::string extra;

        if (cmd == "rot") {
            storeU32(g.mach + 0x54, arg(0));
            eax = UnitAIRotateToAngleState_onEnter(g.self);
        } else if (cmd == "wait") {
            storeU32(g.tmpl + 0xCC, arg(0));
            eax = AIWaitUntilMembersFinishedBeforeExitState_onEnter(g.self);
        } else if (cmd == "upd") {
            storeU32(g.self + 0x20, bits(g.path));
            eax = UnitAICombinedUnitMoveToState_update(g.self);
        } else if (cmd == "uex") {
            if (arg(1) == 0) {
                storeU32(g.own + 0x374, 0);
            }
            eax = UnitAICombinedUnitMoveToState_onExit(g.self, arg(0));
        } else if (cmd == "arc") {
            storeU32(g.mach + 0x2C, arg(1));
            if (arg(0) == 0) {
                storeU32(g.own + 0x374, 0);
            }
            eax = AIRotateFiringarc_onEnter(g.self);
        } else if (cmd == "beach") {
            g_ret[0x0079E530] = arg(0);
            eax = UnitAIDepartBeach_onEnter(g.self);
        } else if (cmd == "drift") {
            storeU32(g.own + 0xA0, arg(0) != 0 ? 0x00020001u : 1u);
            eax = UnitAIDriftState_onEnter(g.self);
            extra = ow(g, 0xA0);
        } else if (cmd == "hent") {
            storeU32(g.own + 0xA0, arg(0) != 0 ? 0x00200001u : 1u);
            eax = UnitAIHordeGarrisonedState_onEnter(g.self);
            extra = ow(g, 0xA0);
        } else if (cmd == "hexit") {
            storeU32(g.own + 0xA0, arg(0) != 0 ? 0x00200001u : 1u);
            eax = UnitAIHordeGarrisonedState_onExit(g.self, 0);
            extra = ow(g, 0xA0);
        } else if (cmd == "hold") {
            g_vret[0x4C] = bits(g.prod);
            eax = UnitAIDisguiseHoldFireState_onEnter(g.self);
            extra = p20(g);
        } else if (cmd == "make") {
            g_ret[0x00416820] = arg(0) == 0 ? bits(g.block) : 0;
            g_ret[0x007A57A0] = bits(g.block);
            eax = AIStateMachine_createLinkedState(g.mach, arg(1));
        } else if (cmd == "board") {
            g_vret[0x4C] = bits(g.prod);
            g_ret[0x005B9F30] = bits(g.goal);
            eax = UnitAIPrepareForBoardingState_onEnter(g.self);
            extra = p20(g);
        } else if (cmd == "goalx") {
            g_ret[0x005B9F30] = arg(0) ? bits(g.goal) : 0;
            g_ret[0x006A7620] = arg(1) ? bits(g.box) : 0;
            if (arg(2) == 0) {
                storeU32(g.own + 0x374, 0);
            }
            if (arg(3) != 0) {
                storeU32(g.self + 0x20, bits(g.sub));
            }
            eax = UnitAIMoveToGoalAndEnterState_onExit(g.self, 0);
            mem = {0x20};
        } else if (cmd == "para") {
            if (arg(0) != 0) {
                storeU32(g.self + 0x20, bits(g.sub));
            }
            eax = UnitAIParachuteToGroundState_onExit(g.self, 0);
            mem = {0x20};
        } else if (cmd == "boardx") {
            if (arg(0) == 0) {
                storeU32(g.own + 0x374, 0);
            }
            g_ret[0x006A7600] = bits(g.phys);
            if (arg(1) != 0) {
                storeU32(g.self + 0x20, bits(g.sub));
            }
            eax = UnitAIPrepareForBoardingState_onExit(g.self, 0);
            mem = {0x20};
        } else if (cmd == "facex") {
            if (arg(0) == 0) {
                g_ret[0x005B9F30] = 0;
            } else {
                g_ret[0x005B9F30] = bits(g.goal);
                if (arg(0) == 1) {
                    g.goal[0x80] = 1;
                } else if (arg(0) == 2) {
                    g.goal[0x471] = 1;
                }
            }
            g_ret[0x00687810] = bits(g.mod);
            g_vret[0x38] = arg(2);
            eax = AIFaceState_onExit(g.self, arg(1));
        } else if (cmd == "melee") {
            g.own[0x80] = static_cast<std::uint8_t>(arg(0));
            g_ret[0x006A7600] = arg(1) ? bits(g.phys) : 0;
            g_vret[0x84] = arg(2) ? bits(g.wpn) : 0;
            eax = AIAttackMeleeHordeWaitState_onExit(g.self, 0);
        } else if (cmd == "horde") {
            g_ret[0x005B9F30] = arg(1) ? bits(g.goal) : 0;
            if (arg(0) != 0) {
                storeU32(g.info + 0xC0, 0x10);
            }
            g_ret[0x006A7600] = arg(2) ? bits(g.phys) : 0;
            eax = AIHordeEnterState_onExit(g.self, arg(3));
        } else if (cmd == "reload") {
            storeU32(g.own + 0xB4, arg(0) != 0 ? 0x21u : 1u);
            eax = UnitAIReloadAmmoState_onExit(g.self, 0);
            extra = ow(g, 0xB4);
        } else if (cmd == "refine") {
            storeU32(g.own + 0xB4, arg(0) != 0 ? 0x00800001u : 1u);
            eax = UnitAIDeliverToRefineryState_onExit(g.self, 0);
            extra = ow(g, 0xB4);
        } else if (cmd == "path") {
            g.mach[0x44] = static_cast<std::uint8_t>(arg(0));
            if (arg(1) == 0) {
                storeU32(g.own + 0x374, 0);
            }
            g_ret[0x00686E50] = 0x3;
            eax = UnitAICreatePathState_onEnter(g.self);
        } else if (cmd == "exitp") {
            storeU32(g.self + 0x24, arg(0));
            if (arg(1) == 0) {
                storeU32(g.own + 0x374, 0);
            }
            eax = AIFollowExitProductionPathState_onExit(g.self, arg(2));
        } else if (cmd == "garx") {
            storeU32(g.own + 0xA0, arg(0) != 0 ? 0x00200001u : 1u);
            if (arg(1) != 0) {
                storeU32(g.self + 0x20, bits(g.sub));
            }
            eax = UnitAIGarrisonedState_onExit(g.self, 0);
            mem = {0x20};
            extra = ow(g, 0xA0);
        } else if (cmd == "wayx") {
            storeU32(g.own + 0xB0, arg(0) != 0 ? 0x00400001u : 1u);
            if (arg(1) == 0) {
                storeU32(g.own + 0x374, 0);
            }
            if (arg(2) != 0) {
                storeU32(g.self + 0x20, bits(g.sub));
            }
            eax = UnitAIMoveOutOfTheWayState_onExit(g.self, 0);
            mem = {0x20};
            extra = ow(g, 0xB0) + " lb=" + hex(g.loco[0x434]) + " l1e8=" + hex8(loadU32(g.loco + 0x1E8)) +
                    " l1ec=" + hex8(loadU32(g.loco + 0x1EC));
        } else if (cmd == "goalox") {
            if (arg(0) == 0) {
                storeU32(g.own + 0x374, 0);
            }
            eax = UnitAIMoveToGoalState_onExit(g.self, arg(1));
        } else if (cmd == "delx") {
            storeU32(g.self + 0x28, arg(1));
            if (arg(0) != 0) {
                const std::uint32_t index = arg(1);
                storeU32(g.own + 0xA0 + static_cast<int>(index >> 5) * 4, 1u << (index & 31));
            }
            eax = UnitAIMoveToDeliverState_onExit(g.self, arg(2));
            extra = "c0=" + hex8(loadU32(g.own + 0xA0)) + " c1=" + hex8(loadU32(g.own + 0xA4));
        } else if (cmd == "harvest") {
            storeU32(g.own + 0xB4, arg(0) != 0 ? 0x40000001u : 1u);
            eax = AIHarvestPrepareSiteState_onExit(g.self, 0);
            extra = ow(g, 0xB4);
        } else if (cmd == "jetx") {
            storeU32(g.own + 0xB0, arg(0) != 0 ? 0x02000001u : 1u);
            if (arg(1) != 0) {
                storeU32(g.self + 0x20, bits(g.sub));
            }
            eax = JetAIAttackMoveToGoalState_onExit(g.self, 0);
            mem = {0x20};
            extra = ow(g, 0xB0);
        } else if (cmd == "atkpos") {
            std::uint32_t bitsA8 = 0x100u;
            if (arg(0) != 0) {
                bitsA8 |= 1u;
            }
            if (arg(1) != 0) {
                bitsA8 |= 4u;
            }
            storeU32(g.own + 0xA8, bitsA8);
            if (arg(2) != 0) {
                storeU32(g.self + 0x20, bits(g.sub));
            }
            eax = UnitAIAttackPositionState_onExit(g.self, 0);
            mem = {0x20};
            extra = "a8=" + hex8(loadU32(g.own + 0xA8));
        } else if (cmd == "docku") {
            if (arg(0) == 0) {
                g_ret[0x005B9F30] = 0;
            } else {
                g_ret[0x005B9F30] = bits(g.goal);
                if (arg(0) == 1) {
                    g.goal[0x80] = 1;
                } else if (arg(0) == 2) {
                    g.goal[0x471] = 1;
                }
            }
            g_ret[0x00687810] = arg(1) ? bits(g.mod) : 0;
            g_vret[0x38] = arg(2);
            g_vret[0x0C] = arg(3);
            eax = AIDockWaitForClearanceState_update(g.self);
        } else if (cmd == "docke") {
            if (arg(0) == 0) {
                g_ret[0x005B9F30] = 0;
            } else {
                g_ret[0x005B9F30] = bits(g.goal);
                if (arg(0) == 1) {
                    g.goal[0x80] = 1;
                }
            }
            g_ret[0x00687810] = arg(1) ? bits(g.mod) : 0;
            storeU32(g.game + 0x58, arg(2));
            g_vret[0x54] = arg(3);
            eax = AIDockProcessDockState_onEnter(g.self);
            mem = {0x28};
        } else if (cmd == "guard") {
            if (arg(0) == 0) {
                storeU32(g.mach + 0x24, 0);
            }
            g_ret[0x00687C30] = arg(1);
            g_ret[0x006AD4B0] = arg(2);
            eax = AIGuardGoingIdleState_onEnter(g.self);
        } else if (cmd == "start") {
            g_ret[0x005B9F30] = arg(0) ? bits(g.goal) : 0;
            g.self[0x20] = static_cast<std::uint8_t>(arg(1));
            g_ret[0x007578F0] = arg(2);
            eax = AIStartAttackObjState_onEnter(g.self);
        } else if (cmd == "delen") {
            storeU32(g.self + 0x20, arg(2));
            storeU32(g.self + 0x28, arg(0));
            if (arg(1) != 0) {
                const std::uint32_t index = arg(0);
                storeU32(g.own + 0xA0 + static_cast<int>(index >> 5) * 4, 1u << (index & 31));
            }
            eax = UnitAIMoveToDeliverState_onEnter(g.self);
            mem = {0x24};
            extra = "c0=" + hex8(loadU32(g.own + 0xA0)) + " c1=" + hex8(loadU32(g.own + 0xA4));
        } else if (cmd == "taxi") {
            g_ret[0x00686CB0] = arg(0) ? bits(g.mod) : 0;
            if (arg(1) == 0) {
                storeU32(g.own + 0x374, 0);
            }
            storeU32(g.own + 0xB4, arg(2) != 0 ? 0x21u : 1u);
            if (arg(3) != 0) {
                storeU32(g.self + 0x20, bits(g.sub));
            }
            eax = UnitAITaxiFromHangerState_onExit(g.self, 0);
            mem = {0x20};
            extra = ow(g, 0xB4) + " m40=" + hex8(loadU32(g.mod + 0x40)) + " lb=" + hex(g.loco[0x434]);
        } else if (cmd == "gidl") {
            g_ret[0x006EFEB0] = arg(0) ? bits(g.host) : 0;
            g_ret[0x006A7600] = arg(1) ? bits(g.phys) : 0;
            g_vret[0x08] = bits(g.inner);
            eax = UnitAIGarrisonedIdleState_onEnter(g.self);
        } else if (cmd == "xsa") {
            storeU32(g.xfer + 4, arg(0));
            eax = AIMoveToStateSA_xfer(g.self, g.xfer);
        } else if (cmd == "xtight") {
            eax = AIMoveAndTightenState_xfer(g.self, g.xfer);
        } else if (cmd == "xcrate") {
            eax = AIPickUpCrateState_xfer(g.self, g.xfer);
        } else if (cmd == "xevac") {
            eax = AIMoveAndEvacuateState_xfer(g.self, g.xfer);
        } else if (cmd == "xdel") {
            eax = AIMoveAndDeleteState_xfer(g.self, g.xfer);
        } else if (cmd == "xatk") {
            storeU32(g.xfer + 4, arg(0));
            storeU32(g.self + 0x6C, arg(1));
            eax = AIAttackFollowWaypointPathState_xfer(g.self, g.xfer);
        } else if (cmd == "xwander") {
            eax = AIWanderState_xfer(g.self, g.xfer);
        } else if (cmd == "xenter") {
            eax = AIEnterState_xfer(g.self, g.xfer);
        } else if (cmd == "xboth") {
            eax = AIEnterAndAttackState_xfer(g.self, g.xfer);
        } else if (cmd == "xposent") {
            eax = AIMoveToPositionAndEnterState_xfer(g.self, g.xfer);
        } else if (cmd == "xapp") {
            eax = AIHarvestApproachSiteState_xfer(g.self, g.xfer);
        } else {
            std::cout << "eax=00000000 kind=bad log=. mem=." << std::endl;
            continue;
        }
        finish(eax, g.self, mem, extra);
    }
    return 0;
}
