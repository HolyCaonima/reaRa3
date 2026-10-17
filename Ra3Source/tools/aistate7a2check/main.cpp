#include "ra3/gamelogic/AIState7A2.h"

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
std::uint32_t g_vret[0x200]{};
std::uint32_t g_fill = 0;
int g_ver = -1;
int g_poke = 0;
int g_other84 = 0;
int g_noWeapon = 0;
std::uint32_t g_stack = 0;

struct Tag {
    std::uint8_t* base = nullptr;
    std::uint32_t size = 0;
    const char* name = "";
};

struct Graph {
    std::uint8_t* self = nullptr;
    std::uint8_t* mach = nullptr;
    std::uint8_t* own = nullptr;
    std::uint8_t* tmpl = nullptr;
    std::uint8_t* loco = nullptr;
    std::uint8_t* goal = nullptr;
    std::uint8_t* info = nullptr;
    std::uint8_t* xfer = nullptr;
    std::uint8_t* game = nullptr;
    std::uint8_t* sub = nullptr;
    std::uint8_t* spot = nullptr;
    std::uint8_t* pad = nullptr;
    std::uint8_t* phys = nullptr;
    std::uint8_t* weapon = nullptr;
    std::uint8_t* other = nullptr;
    std::uint8_t* block = nullptr;
    std::uint8_t* reg = nullptr;
    std::uint8_t* glob = nullptr;
    std::uint8_t* id = nullptr;
    std::uint8_t* node = nullptr;
    std::uint8_t* team = nullptr;
    std::uint8_t* player = nullptr;
    std::uint8_t* form = nullptr;
    std::uint8_t* ammo = nullptr;
    std::uint8_t* module = nullptr;
    std::uint8_t* site = nullptr;
    std::uint8_t* rec = nullptr;
    std::uint8_t* vtSelf = nullptr;
    std::uint8_t* vtMach = nullptr;
    std::uint8_t* vtLoco = nullptr;
    std::uint8_t* vtXfer = nullptr;
    std::uint8_t* vtSub = nullptr;
    std::uint8_t* vtSpot = nullptr;
    std::uint8_t* vtPhys = nullptr;
    std::uint8_t* vtWeapon = nullptr;
    std::uint8_t* vtReg = nullptr;
    std::uint8_t* vtGlob = nullptr;
    std::uint8_t* vtForm = nullptr;
    std::uint8_t* vtSite = nullptr;
};

Graph g;
std::vector<Tag> g_tags;
std::map<std::uint32_t, std::uint32_t> g_abs;

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

bool onStack(std::uint32_t value) {
    switch (value) {
    case 0x00BF21D8u:
    case 0x00C2F360u:
    case 0x00C2FF28u:
    case 0x00C31A98u:
    case 0x00CC785Cu:
    case 0x7F7FFFFFu:
    case 0x7FFFFFFFu:
        return false;
    default:
        break;
    }
    const std::uint32_t base = g_stack - 0x8000u;
    return value >= base && value < g_stack + 0x1000u;
}

std::string fmt(std::uint32_t value) {
    if (onStack(value)) {
        return "s";
    }
    for (const Tag& tag : g_tags) {
        const std::uint32_t base = bits(tag.base);
        if (tag.base != nullptr && value >= base && value - base < tag.size) {
            const std::uint32_t off = value - base;
            return off == 0 ? tag.name : std::string(tag.name) + "+" + hex(off);
        }
    }
    return hex(value);
}

std::string kindOf(std::uint32_t value) {
    if (onStack(value)) {
        return "s";
    }
    for (const Tag& tag : g_tags) {
        if (tag.base != nullptr && value == bits(tag.base)) {
            return tag.name;
        }
    }
    return hex(value);
}

void logV(int slot, const std::uint32_t* args, int nargs) {
    std::ostringstream text;
    text << "v" << std::hex << std::nouppercase << slot;
    for (int i = 0; i < nargs; ++i) {
        text << ":" << fmt(args[i]);
    }
    addLog(text.str());
}

std::uint32_t retOf(void* self, int slot) {
    if (self == g.phys && slot == 0x84) {
        return g_noWeapon ? 0 : bits(g.weapon);
    }
    if (self == g.other && slot == 0x84) {
        return static_cast<std::uint32_t>(g_other84);
    }
    return g_vret[slot];
}

#define V0(name, slot)                                                                 \
    std::uint32_t __fastcall name(void* self, void*) {                                 \
        logV(slot, nullptr, 0);                                                        \
        return retOf(self, slot);                                                      \
    }
#define V1(name, slot)                                                                 \
    std::uint32_t __fastcall name(void* self, void*, std::uint32_t a0) {               \
        logV(slot, &a0, 1);                                                            \
        return retOf(self, slot);                                                      \
    }
#define V2(name, slot)                                                                 \
    std::uint32_t __fastcall name(void* self, void*, std::uint32_t a0, std::uint32_t a1) { \
        const std::uint32_t args[2] = {a0, a1};                                        \
        logV(slot, args, 2);                                                           \
        return retOf(self, slot);                                                      \
    }
#define V3(name, slot)                                                                 \
    std::uint32_t __fastcall name(void* self, void*, std::uint32_t a0, std::uint32_t a1, \
                                  std::uint32_t a2) {                                  \
        const std::uint32_t args[3] = {a0, a1, a2};                                    \
        logV(slot, args, 3);                                                           \
        return retOf(self, slot);                                                      \
    }

V0(v00, 0x00)
V0(v08, 0x08)
V0(v0c, 0x0C)
V0(v10, 0x10)
V0(v3c0, 0x3C)
V1(v3c1, 0x3C)
V0(v48, 0x48)
V0(v4c, 0x4C)
V0(v84, 0x84)
V0(v244, 0x244)
V0(v1c4, 0x1C4)
V0(v04z, 0x04)
V1(v04, 0x04)
V1(v40f, 0x40)
V1(v84f, 0x84)
V3(v94c, 0x94)
V1(v1c, 0x1C)
V1(v20, 0x20)
V1(v38, 0x38)
V1(v44, 0x44)
V1(v58, 0x58)
V1(v60, 0x60)
V1(v64, 0x64)
V1(v6c, 0x6C)
V1(v70, 0x70)
V1(v94, 0x94)
V1(v98, 0x98)
V1(v288, 0x288)
V2(v14, 0x14)
V2(v40, 0x40)
V2(v158, 0x158)
V2(v00b, 0x00)
V3(v7c, 0x7C)

std::uint32_t __fastcall v84field(void* self, void* edx, std::uint32_t arg) {
    const std::uint32_t result = v84f(self, edx, arg);
    if (g_poke > 0 && arg != 0) {
        *reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(arg)) = static_cast<std::uint8_t>(g_poke);
    }
    return result;
}

std::uint32_t __fastcall v18(void*, void*, std::uint32_t arg) {
    auto* bytes = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(arg));
    std::ostringstream text;
    text << "v18:" << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
         << static_cast<unsigned>(bytes[0]) << std::setw(2) << static_cast<unsigned>(bytes[1]);
    addLog(text.str());
    if (g_ver >= 0) {
        bytes[1] = static_cast<std::uint8_t>(g_ver);
    }
    return g_vret[0x18];
}

std::uint32_t __fastcall v14out(void*, void*, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const std::uint32_t args[3] = {a0, a1, a2};
    logV(0x14, args, 3);
    if (a2 != 0) {
        auto* out = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(a2));
        storeU32(out, 0x11111111u);
        storeU32(out + 4, 0x22222222u);
        storeU32(out + 8, 0x33333333u);
    }
    return g_vret[0x14];
}

void putFn(std::uint8_t* table, int slot, void* fn) {
    storeU32(table + slot, bits(fn));
}

std::uint32_t __cdecl onDirect(std::uint32_t address, void* ecx, const std::uint32_t* args, int nargs) {
    std::ostringstream text;
    text << "d" << std::hex << std::nouppercase << address << ":" << fmt(bits(ecx));
    for (int i = 0; i < nargs; ++i) {
        text << ":" << fmt(args[i]);
    }
    addLog(text.str());
    if (address == 0x006E4760 && nargs >= 2 && args[1] != 0) {
        storeU32(reinterpret_cast<void*>(static_cast<std::uintptr_t>(args[1])), g_fill);
    }
    const auto found = g_ret.find(address);
    return found == g_ret.end() ? 0 : found->second;
}

std::uint32_t onAbs(std::uint32_t address) {
    const auto found = g_abs.find(address);
    return found == g_abs.end() ? 0 : found->second;
}

std::uint8_t* alloc(std::uint32_t size) {
    auto* memory = new std::uint8_t[size]();
    return memory;
}

std::uint8_t* allocAligned(std::uint32_t size) {
    auto* memory = static_cast<std::uint8_t*>(_aligned_malloc(size, 256));
    std::memset(memory, 0, size);
    return memory;
}

void tag(std::uint8_t* base, std::uint32_t size, const char* name) {
    g_tags.push_back(Tag{base, size, name});
}

void build() {
    g_tags.clear();
    g = {};
    auto vt = [] { return alloc(0x300); };
    g.vtSelf = vt();
    g.vtMach = vt();
    g.vtLoco = vt();
    g.vtXfer = vt();
    g.vtSub = vt();
    g.vtSpot = vt();
    g.vtPhys = vt();
    g.vtWeapon = vt();
    g.vtReg = vt();
    g.vtGlob = vt();
    g.vtForm = vt();
    g.vtSite = vt();
    putFn(g.vtSelf, 0x3C, &v3c0);
    putFn(g.vtSelf, 0x4C, &v4c);
    putFn(g.vtMach, 0x1C, &v1c);
    putFn(g.vtMach, 0x38, &v38);
    putFn(g.vtLoco, 0x1C4, &v1c4);
    putFn(g.vtLoco, 0x244, &v244);
    putFn(g.vtLoco, 0x288, &v288);
    putFn(g.vtXfer, 0x04, &v04);
    putFn(g.vtXfer, 0x08, &v08);
    putFn(g.vtXfer, 0x14, &v14);
    putFn(g.vtXfer, 0x18, &v18);
    putFn(g.vtXfer, 0x20, &v20);
    putFn(g.vtXfer, 0x40, &v40f);
    putFn(g.vtXfer, 0x44, &v44);
    putFn(g.vtXfer, 0x58, &v58);
    putFn(g.vtXfer, 0x60, &v60);
    putFn(g.vtXfer, 0x64, &v64);
    putFn(g.vtXfer, 0x6C, &v6c);
    putFn(g.vtXfer, 0x70, &v70);
    putFn(g.vtXfer, 0x84, &v84field);
    putFn(g.vtSub, 0x00, &v00);
    putFn(g.vtSub, 0x10, &v10);
    putFn(g.vtSub, 0x1C, &v1c);
    putFn(g.vtSub, 0x38, &v38);
    putFn(g.vtSpot, 0x14, &v14out);
    putFn(g.vtSpot, 0x3C, &v3c1);
    putFn(g.vtPhys, 0x40, &v40);
    putFn(g.vtPhys, 0x84, &v84);
    putFn(g.vtPhys, 0x94, &v94c);
    putFn(g.vtPhys, 0x98, &v98);
    putFn(g.vtWeapon, 0x7C, &v7c);
    putFn(g.vtReg, 0x6C, &v6c);
    putFn(g.vtReg, 0x158, &v158);
    putFn(g.vtGlob, 0x00, &v00b);
    putFn(g.vtForm, 0x94, &v94);
    putFn(g.vtSite, 0x04, &v04z);
    putFn(g.vtSite, 0x0C, &v0c);
    putFn(g.vtSite, 0x48, &v48);

    auto obj = [](std::uint32_t size, std::uint8_t* table) {
        auto* memory = alloc(size);
        if (table != nullptr) {
            storeU32(memory, bits(table));
        }
        return memory;
    };
    g.self = obj(0x100, g.vtSelf);
    g.mach = obj(0x90, g.vtMach);
    g.own = obj(0x500, nullptr);
    g.tmpl = obj(0x140, nullptr);
    g.loco = obj(0x450, g.vtLoco);
    g.goal = obj(0x200, nullptr);
    g.info = obj(0x140, nullptr);
    g.xfer = obj(0x40, g.vtXfer);
    g.game = obj(0x80, nullptr);
    g.sub = obj(0x90, g.vtSub);
    g.spot = obj(0x40, g.vtSpot);
    g.pad = obj(0x40, nullptr);
    g.phys = obj(0xA0, g.vtPhys);
    g.weapon = obj(0x40, g.vtWeapon);
    g.other = obj(0xA0, g.vtPhys);
    g.block = obj(0x90, nullptr);
    g.reg = obj(0x40, g.vtReg);
    g.glob = obj(0x10, nullptr);
    storeU32(g.glob, bits(reinterpret_cast<void*>(&v00b)));
    g.id = obj(0x10, nullptr);
    g.node = allocAligned(0x20);
    g.team = obj(0x300, nullptr);
    g.player = obj(0xA0, nullptr);
    g.form = obj(0x20, g.vtForm);
    g.ammo = obj(0x20, nullptr);
    g.module = obj(0xC0, nullptr);
    g.site = obj(0x50, g.vtSite);
    g.rec = obj(0x40, nullptr);
    storeU32(g.self + 0x18, bits(g.mach));
    storeU32(g.mach + 0x24, bits(g.own));
    storeU32(g.mach + 0x28, 0x28);
    storeU32(g.own + 4, bits(g.tmpl));
    storeU32(g.own + 0x374, bits(g.loco));
    storeU32(g.loco + 4, bits(g.tmpl));
    storeU32(g.loco + 0x214, bits(g.module));
    storeU32(g.own + 0x33C, bits(g.form));
    storeU32(g.own + 0x37C, bits(g.ammo));
    storeU32(g.own + 0xF4, bits(g.ammo));
    storeU32(g.goal + 4, bits(g.info));
    storeU32(g.pad + 4, bits(g.info));
    storeU32(g.sub + 0x68, 0x68);
    storeU32(g.player + 0x88, bits(g.player + 0x88));
    storeU32(g.self + 8, bits(g.self + 8));
    g.tmpl[0x54] = 0;
    storeU32(g.tmpl + 0x54, 0x3F800000u);
    const Tag rows[] = {
        {g.self, 0x100, "self"}, {g.mach, 0x90, "mach"}, {g.own, 0x500, "own"},
        {g.tmpl, 0x140, "tmpl"}, {g.loco, 0x450, "loco"}, {g.goal, 0x200, "goal"},
        {g.info, 0x140, "info"}, {g.xfer, 0x40, "xfer"}, {g.game, 0x80, "game"},
        {g.sub, 0x90, "sub"}, {g.spot, 0x40, "spot"}, {g.pad, 0x40, "pad"},
        {g.phys, 0xA0, "phys"}, {g.weapon, 0x40, "wpn"}, {g.other, 0xA0, "other"},
        {g.block, 0x90, "block"}, {g.reg, 0x40, "reg"}, {g.glob, 0x10, "glob"},
        {g.id, 0x10, "id"}, {g.node, 0x20, "node"}, {g.team, 0x300, "team"},
        {g.player, 0xA0, "player"}, {g.form, 0x20, "form"}, {g.ammo, 0x20, "ammo"},
        {g.module, 0xC0, "mod"}, {g.site, 0x50, "site"}, {g.rec, 0x40, "rec"},
    };
    g_tags.assign(rows, rows + sizeof(rows) / sizeof(rows[0]));
    g_abs.clear();
    g_abs[0] = bits(g.glob);
    g_abs[0x00DEBA64] = bits(g.game);
    g_abs[0x00BE700C] = 0x7F7FFFFF;
    g_abs[0x00BD9628] = 0xBF800000;
    g_abs[0x00CC561C] = 0x00CC785C;
    g_abs[0x00DF03FC] = 0;
    g_abs[0x00DFC190] = 0;
    g_ret.clear();
    std::memset(g_vret, 0, sizeof(g_vret));
    g_vret[0x4C] = bits(g.sub);
    g_vret[0x10] = 0x55;
    g_vret[0x14] = 1;
    g_vret[0x1C] = 0x41;
    g_vret[0x244] = 1;
    g_vret[0x288] = 0x19;
    g_vret[0x1C4] = bits(g.site);
    g_vret[0x48] = 5;
    g_vret[0x0C] = 9;
    g_vret[0x04] = 3;
    g_vret[0x84] = 0x84;
    g_vret[0x08] = 0x08;
    g_vret[0x6C] = 0x16;
    g_vret[0x3C] = 0;
    g_fill = bits(g.pad);
    g_ver = -1;
    g_poke = 0;
    g_other84 = 0;
    g_noWeapon = 0;
    g_ret[0x006A51A0] = bits(g.goal);
    g_ret[0x007A0B30] = 1;
    g_ret[0x006D2020] = 0x21;
    g_ret[0x006A4FB0] = 0x46;
    g_ret[0x005B9F30] = bits(g.goal);
    g_ret[0x006A4F60] = 1;
    g_ret[0x00416820] = bits(g.block);
    g_ret[0x007A1BD0] = bits(g.block);
    g_ret[0x008663B0] = bits(g.id);
    g_ret[0x0067F130] = bits(g.rec);
    g_ret[0x006A7600] = bits(g.phys);
    g_ret[0x006A7620] = bits(g.other);
    g_ret[0x0079CC50] = 1;
    g_ret[0x007985D0] = 0x77;
    g_ret[0x007527C0] = 1;
    g_ret[0x007EBE30] = 1;
    g_ret[0x007A1570] = 1;
    g_ret[0x007A1400] = 1;
    g_ret[0x006CFCA0] = 4;
    g_ret[0x006E4760] = bits(g.spot);
    g_ret[0x00687560] = 1;
    g_ret[0x0079EAD0] = 3;
    g_ret[0x006A76A0] = 1;
    g_ret[0x007A1FC0] = 0x11;
    g_ret[0x006A4500] = 0x45;
}

std::uint32_t argAt(const std::vector<std::uint32_t>& args, int index) {
    return index < static_cast<int>(args.size()) ? args[index] : 0;
}

std::string memOf() {
    auto one = [](const char* name, std::uint8_t* base, int off) {
        return std::string(name) + hex(off) + "=" + fmt(loadU32(base + off));
    };
    std::ostringstream text;
    text << one("s", g.self, 0x20) << "," << one("s", g.self, 0x24) << "," << one("s", g.self, 0x28)
         << "," << one("s", g.self, 0x2C) << "," << one("s", g.self, 0x30) << "," << one("s", g.self, 0x34)
         << "," << one("s", g.self, 0x38) << "," << one("s", g.self, 0x50) << "," << one("s", g.self, 0x54)
         << "," << one("s", g.self, 0x60)
         << "," << one("o", g.own, 0xA0) << "," << one("o", g.own, 0xA8) << "," << one("o", g.own, 0xB0)
         << "," << one("o", g.own, 0xB4) << "," << one("o", g.own, 0xD4) << "," << one("l", g.loco, 0x1F0)
         << "," << one("l", g.loco, 0x17C)          << "," << one("l", g.loco, 0x21C) << ",b24=" << hex(g.self[0x24])
         << ",b3c=" << hex(g.self[0x3C]) << ",u7c=" << hex(g.sub[0x7C])
         << ",l434=" << hex(g.loco[0x434]) << ",ma4=" << hex(g.module[0xA4])
         << ",m40=" << fmt(loadU32(g.module + 0x40));
    return text.str();
}

void finish(std::uint32_t eax) {
    std::cout << "eax=00000000 kind=" << kindOf(eax) << " log=" << (g_log.empty() ? "." : g_log)
              << " mem=" << memOf() << std::endl;
}

using namespace ra3::gamelogic;

}  // namespace

int main() {
    int stackMark = 0;
    g_stack = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&stackMark));
    AIState7A2Calls calls;
    calls.direct = &onDirect;
    calls.absDword = &onAbs;
    setAIState7A2Calls(calls);
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
        std::vector<std::uint32_t> args;
        std::string token;
        while (input >> token) {
            args.push_back(static_cast<std::uint32_t>(std::strtoul(token.c_str(), nullptr, 0)));
        }
        const auto a = [&](int index) { return argAt(args, index); };
        build();
        g_log.clear();
        std::uint32_t eax = 0;
        if (cmd == "pfw") {
            if (a(0) == 0) {
                g_ret[0x006A51A0] = 0;
            }
            storeU32(g.tmpl + 0xC0, a(1));
            storeU32(g.tmpl + 0xCC, a(2));
            if (a(3) & 1u) g_ret[0x00685EE0] = 1;
            if (a(3) & 2u) g_ret[0x00685EC0] = 1;
            if (a(3) & 4u) g_ret[0x006FFB00] = 1;
            if (a(3) & 8u) g_ret[0x007A0B30] = 0;
            if (a(3) & 16u) g_ret[0x00688810] = 1;
            if (a(3) & 32u) storeU32(g.own + 0x84, 0x100);
            if (a(3) & 64u) g.game[0x58] = 1;
            eax = AIAttackPositionFireWeaponState_onEnter(g.self);
        } else if (cmd == "pfu") {
            if (a(0) == 0) g_ret[0x006A51A0] = 0;
            if (a(0) == 2) g.own[0x471] = 1;
            if (a(0) == 3) storeU32(g.own + 0x88, 1u << 21);
            if (a(0) == 4) g.self[0x24] = 1;
            if (a(0) == 5) g_ret[0x00688810] = 1;
            if (a(0) == 6) g_ret[0x00688810] = 2;
            storeU32(g.self + 0x20, bits(g.sub));
            eax = AIAttackPositionFireWeaponState_update(g.self);
        } else if (cmd == "gid") {
            if (a(0) == 1 || a(0) == 2 || a(0) == 3) g_ret[0x006A6210] = bits(g.goal);
            if (a(0) == 1) {
                g_ret[0x00687CB0] = bits(g.spot);
                g_ret[0x00A780D0] = 1;
            }
            if (a(0) == 2) g_vret[0x244] = 0;
            if (a(0) == 3) g_ret[0x006906A0] = 3;
            if (a(0) == 4) g.self[0x20] = 1;
            if (a(0) == 5) g_ret[0x006A6F60] = 1;
            if (a(0) == 0) {
                g_ret[0x00687C30] = bits(g.goal);
                g_ret[0x006AD4B0] = 1;
            }
            eax = AIGuardIdleState_update(g.self);
        } else if (cmd == "hws") {
            if (a(0) == 0) g_vret[0x1C4] = 0;
            if (a(0) == 1) g_ret[0x005B9F30] = 0;
            if (a(0) == 2) g_ret[0x0067F130] = 0;
            if (a(0) == 3) storeU32(g.rec + 0x38, 1);
            storeU32(g.goal + 0xDC, 0x11);
            storeU32(g.own + 0xDC, 0x22);
            storeU32(g.rec + 0x30, a(1) ? 1 : 100);
            g_vret[0x0C] = a(1) ? 50 : 4;
            g_vret[0x04] = 1;
            if (a(2)) storeU32(g.own + 0xB4, 0x80000000u);
            g.game[0x58] = 7;
            eax = AIHarvestWorkSiteState_onEnter(g.self);
        } else if (cmd == "hen") {
            if (a(0) == 0) storeU32(g.mach + 0x24, 0);
            if (a(0) == 1) g_ret[0x005B9F30] = 0;
            if (a(0) == 2) g.goal[0x471] = 1;
            if (a(0) == 3) g_ret[0x006A7600] = 0;
            if (a(0) == 4) g_ret[0x006A7620] = 0;
            if (a(0) == 5) g_other84 = 1;
            if (a(0) == 8) g_noWeapon = 1;
            if (a(0) == 6) g_vret[0x94] = 0;
            if (a(0) != 5 && a(0) != 6) g_vret[0x94] = 1;
            if (a(0) == 7) storeU32(g.info + 0xC0, 0x10);
            eax = AIHordeEnterState_onEnter(g.self);
        } else if (cmd == "gon") {
            if (a(0) == 0) storeU32(g.mach + 0x24, 0);
            if (a(0) == 1) {
                g_ret[0x00687C30] = bits(g.goal);
                g_ret[0x006AD4B0] = 1;
            }
            if (a(0) == 2) g_ret[0x00687CB0] = bits(g.spot);
            eax = AIGoingIdleState_onEnter(g.self);
        } else if (cmd == "wff") {
            if (a(0) == 0) g_ret[0x006A51A0] = 0;
            if (a(0) == 1) g_ret[0x00688810] = 0;
            if (a(0) == 2) {
                storeU32(g.own + 0x84, 0x100);
                g.game[0x58] = 1;
                g_ret[0x00688810] = 0;
            }
            if (a(0) == 3) {
                g_ret[0x00688810] = 1;
                g_ret[0x006864D0] = 4;
            }
            g.game[0x58] = a(0) == 2 ? 1 : 9;
            eax = AIWaitUntilFinishedFiringState_onEnter(g.self);
        } else if (cmd == "pfb") {
            g_ret[0x0063EB60] = a(0) ? 1 : 0;
            if (a(1) == 0) g_ret[0x005B9F30] = 0;
            g_ret[0x00658A80] = a(2) ? 1 : 0;
            eax = AIPrepareForBoarding_onEnter(g.self);
        } else if (cmd == "fex") {
            storeU32(g.loco + 0x218, 0x1234);
            g_ret[0x007527C0] = a(0) ? 1 : 0;
            eax = AIFollowExitProductionPathState_onEnter(g.self);
        } else if (cmd == "rld") {
            if (a(0)) storeU32(g.own + 0xB4, 0x20);
            if (a(1) == 0) storeU32(g.own + 0x37C, 0);
            g.game[0x58] = 9;
            eax = UnitAIReloadAmmoState_onEnter(g.self);
        } else if (cmd == "mow") {
            if (a(0)) storeU32(g.own + 0xB0, 1u << 22);
            eax = UnitAIMoveOutOfTheWayState_onEnter(g.self);
        } else if (cmd == "jla") {
            g_ret[0x006E4760] = a(0) ? 0 : bits(g.spot);
            g_vret[0x3C] = a(1);
            if (a(2) == 0) storeU32(g.loco + 0x214, 0);
            g_vret[0x14] = a(3) ? 0 : 1;
            storeU32(g.module + 0x40, 0xFFFFFFFF);
            eax = JetAILandingApproachState_onEnter(g.self);
        } else if (cmd == "off") {
            storeU32(g.team + 0x28C, 3);
            g_ret[0x007EBE30] = a(0) ? 0 : 1;
            g_ret[0x007E6AF0] = a(1);
            if (a(2) == 1) {
                storeU32(g.node, bits(g.player + 0x88));
                storeU32(g.node + 8, bits(g.rec));
                storeU32(g.rec + 0x0C, bits(g.info));
                storeU32(g.info + 4, bits(g.team));
                storeU32(g.player + 0x88, bits(g.node));
            } else if (a(2) == 2) {
                storeU32(g.node, bits(g.player + 0x88));
                storeU32(g.node + 8, bits(g.rec));
                storeU32(g.rec + 0x0C, bits(g.info));
                storeU32(g.info + 4, 1);
                storeU32(g.player + 0x88, bits(g.node));
            }
            g_ret[0x007A1570] = a(3) ? 0 : 1;
            g_ret[0x007A1400] = a(4) ? 0 : 1;
            eax = AIPlayer_offerTeam(g.player, bits(g.team));
        } else if (cmd == "cid") {
            if (a(0) == 0) g_ret[0x005B9F30] = 0;
            if (a(0) == 1) storeU32(g.own + 0xD4, 0x10);
            if (a(0) == 2) storeU32(g.goal + 0xD4, 8);
            eax = UnitAICombinedUnitIdleState_onEnter(g.self);
        } else if (cmd == "egl") {
            if (a(0) == 0) g_ret[0x005B9F30] = 0;
            if (a(0) == 2) storeU32(g.loco + 0x214, 0);
            storeU32(g.module + 0x40, 0x10);
            eax = UnitAIEnterGoalState_onEnter(g.self);
        } else if (cmd == "pke") {
            if (a(0) == 1) storeU32(g.tmpl + 0xC8, 0x100);
            if (a(0) == 2) storeU32(g.own + 0xB4, 0x20);
            g_ret[0x0079CC50] = a(0) == 3 ? 0 : 1;
            storeU32(g.mach + 0x54, a(0) == 4 ? 0x7F7FFFFF : 0x3F800000);
            eax = UnitAIParkOrientState_onEnter(g.self);
        } else if (cmd == "pkx") {
            if (a(0)) storeU32(g.own + 0xB4, 0x20);
            if (a(1) == 0) storeU32(g.own + 0x374, 0);
            storeU32(g.loco + 0x21C, a(1) == 2 ? 3 : 1);
            eax = UnitAIParkOrientState_onExit(g.self, a(2));
        } else if (cmd == "lnd") {
            g.self[0x3C] = a(0) ? 0 : 1;
            g_ret[0x006E4760] = a(1) ? 0 : bits(g.spot);
            g_fill = a(2) ? 0 : bits(g.pad);
            if (a(3)) storeU32(g.info + 0xC0, 0x10);
            g_ret[0x00687560] = a(4) ? 0 : 1;
            g_vret[0x14] = a(5) ? 0 : 1;
            g_ret[0x0079CEE0] = 0x33;
            storeU32(g.own + 0xDC, 0x22);
            eax = UnitAILandState_update(g.self);
        } else if (cmd == "tku") {
            storeU32(g.self + 0x38, a(0));
            storeU32(g.self + 0x20, 0x10);
            g_ret[0x0079CEE0] = a(1) ? 0xFFFFFFFFu : 0;
            if (a(2)) storeU32(g.own + 0xB4, 0x10);
            eax = UnitAITakeOffState_update(g.self);
        } else if (cmd == "tng") {
            storeU32(g.self + 0x20, a(0) ? 0 : bits(g.sub));
            g_ret[0x006D21B0] = a(1) ? 1 : 0;
            if (a(2)) storeU32(g.tmpl + 0xC0, 1u << 26);
            eax = AITunnelNetworkGuardState_update(g.self);
        } else if (cmd == "mpx") {
            if (a(0) & 1u) storeU32(g.own + 0x90, 1u << 29);
            if (a(0) & 2u) storeU32(g.own + 0x88, 1u << 14);
            if (a(0) & 4u) storeU32(g.own + 0xB0, 0xE0000000u);
            if (a(0) & 8u) g_abs[0x00DF03FC] = bits(g.reg);
            storeU32(g.self + 0x20, a(1));
            if (a(2) == 0) storeU32(g.own + 0x374, 0);
            if (a(2) == 2) storeU32(g.loco + 0x214, 0);
            g.module[0xA4] = 1;
            eax = UnitAIMoveOnPathState_onExit(g.self, 0);
        } else if (cmd == "par") {
            if (a(0)) storeU32(g.own + 0xA0, 1u << 19);
            eax = UnitAIParachuteToGroundState_onEnter(g.self);
        } else if (cmd == "amg") {
            if (a(0)) storeU32(g.own + 0xB0, 1u << 25);
            storeU32(g.mach + 0x54, 0x3F800000);
            eax = UnitAIAttackMoveToGoalState_onEnter(g.self);
        } else if (cmd == "aps") {
            storeU32(g.own + 0xA8, a(0));
            eax = UnitAIAttackPositionState_onEnter(g.self);
        } else if (cmd == "jag") {
            g.mach[0x7C] = 7;
            if (a(0)) storeU32(g.own + 0xB0, 1u << 25);
            storeU32(g.mach + 0x54, 0x40000000);
            eax = JetAIAttackMoveToGoalState_onEnter(g.self);
        } else if (cmd == "xim") {
            storeU32(g.xfer + 4, a(0));
            if (a(1)) g_abs[0x00DF03FC] = bits(g.reg);
            eax = AIInternalMoveToState_xfer(g.self, g.xfer);
        } else if (cmd == "xfw") {
            storeU32(g.xfer + 4, a(0));
            storeU32(g.self + 0x60, a(1) ? bits(g.goal) : 0);
            storeU32(g.goal + 4, 0x42);
            if (a(0) & 2u) g_abs[0x00DFC190] = bits(g.reg);
            eax = AIFollowWaypointPathState_xfer(g.self, g.xfer);
        } else if (cmd == "xap") {
            storeU32(g.xfer + 4, a(0));
            eax = AIAttackApproachTargetState_xfer(g.self, g.xfer);
        } else if (cmd == "xpa") {
            storeU32(g.xfer + 4, a(0));
            g_ver = static_cast<int>(a(1)) - 1;
            if (g_ver < 0) g_ver = -1;
            eax = AIAttackPositionApproachTargetState_xfer(g.self, g.xfer);
        } else if (cmd == "xpu") {
            storeU32(g.xfer + 4, a(0));
            eax = AIAttackPursueTargetState_xfer(g.self, g.xfer);
        } else if (cmd == "xga") {
            storeU32(g.self + 0x20, a(0) ? bits(g.sub) : 0);
            g_ret[0x00416820] = a(1) ? 0 : bits(g.block);
            g_ret[0x007A1BD0] = a(2) ? 0 : bits(g.block);
            g_poke = static_cast<int>(a(3));
            eax = AIGarrisonedState_xfer(g.self, g.xfer);
        } else if (cmd == "xmv") {
            storeU32(g.xfer + 4, a(0));
            storeU32(g.self + 0x58, bits(g.goal));
            eax = AIAttackMoveToState_xfer(g.self, g.xfer);
        } else if (cmd == "xfd") {
            storeU32(g.xfer + 4, a(0));
            eax = AIAttackFireDuringApproachState_xfer(g.self, g.xfer);
        } else if (cmd == "xex") {
            storeU32(g.xfer + 4, a(0));
            storeU32(g.self + 0x50, a(1) ? bits(g.goal) : 0);
            storeU32(g.goal + 4, 0x42);
            if (a(0) & 2u) g_abs[0x00DFC190] = bits(g.reg);
            eax = AIFollowWaypointPathExactState_xfer(g.self, g.xfer);
        } else if (cmd == "xtn") {
            storeU32(g.xfer + 4, a(0));
            eax = AITNGuardReturnState_xfer(g.self, g.xfer);
        } else if (cmd == "xbk") {
            eax = AIBackAwayState_xfer(g.self, g.xfer);
        } else if (cmd == "xfp") {
            storeU32(g.xfer + 4, a(0));
            eax = AIFollowPathState_xfer(g.self, g.xfer);
        } else if (cmd == "xmh") {
            storeU32(g.xfer + 4, a(0));
            eax = AIAttackMeleeHordeApproachTargetState_xfer(g.self, g.xfer);
        } else if (cmd == "xsq") {
            storeU32(g.xfer + 4, a(0));
            eax = AIAttackMeleeSquishState_xfer(g.self, g.xfer);
        } else if (cmd == "xch") {
            eax = AIChargeTargetState_xfer(g.self, g.xfer);
        } else if (cmd == "xft") {
            storeU32(g.xfer + 4, a(0));
            storeU32(g.self + 0x60, a(1) ? bits(g.goal) : 0);
            eax = AIFollowPathAsTeamState_xfer(g.self, g.xfer);
        } else if (cmd == "xme") {
            storeU32(g.xfer + 4, a(0));
            storeU32(g.self + 0x50, a(1) ? bits(g.sub) : 0);
            storeU32(g.self + 0x54, 0x54);
            storeU32(g.self + 0x78, 0x78);
            g_ret[0x00416820] = a(2) ? 0 : bits(g.block);
            eax = AIAttackMeleeEngageState_xfer(g.self, g.xfer);
        } else if (cmd == "xai") {
            storeU32(g.xfer + 4, a(0));
            if (a(1)) {
                storeU32(g.node, bits(g.self + 8));
                storeU32(g.node + 8, a(2) ? bits(g.goal) : 0);
                storeU32(g.self + 8, bits(g.node));
            }
            eax = AI_xfer(g.self, g.xfer);
        } else {
            std::cout << "eax=00000000 kind=bad log=. mem=." << std::endl;
            continue;
        }
        finish(eax);
    }
    return 0;
}
