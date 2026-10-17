#include "ra3/gamelogic/ObjectModules74.h"

#include <cstdint>
#include <initializer_list>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::string g_log;
std::uint32_t g_q[16]{};
int g_qn = 0;
int g_qi = 0;
bool g_fill110 = false;
bool g_zero84 = false;
bool g_set84 = false;
std::uint32_t g_out0 = 0;
std::uint32_t g_out1 = 0;
void* g_produced = nullptr;

const std::uint32_t kScatter[8] = {0x155, 0x156, 0x157, 0x15B, 0x158, 0x159, 0x15A, 0x15C};

void resetProbe() {
    g_log.clear();
    g_qn = 0;
    g_qi = 0;
    g_fill110 = false;
    g_zero84 = false;
    g_set84 = false;
    g_out0 = 0;
    g_out1 = 0;
    g_produced = nullptr;
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

std::uint32_t popq() {
    if (g_qi >= g_qn) {
        return 0;
    }
    return g_q[g_qi++];
}

bool popSlot(int slot) {
    return slot == 0x00 || slot == 0xB0 || slot == 0x10C;
}

std::string argText(std::uint32_t arg) {
    std::ostringstream text;
    text << std::hex << std::nouppercase;
    if (arg < 0x10000u) {
        text << ":" << arg;
    } else if (arg == 0x00C2240Cu) {
        text << ":c2240c";
    } else {
        text << ":p";
    }
    return text.str();
}

std::uint32_t onDirect(const char* tag, const std::uint32_t* args, int count, bool pop) {
    std::ostringstream text;
    text << "c" << tag;
    for (int i = 0; i < count; ++i) {
        text << argText(args[i]);
    }
    addLog(text.str());
    return pop ? popq() : 0;
}

std::uint32_t onVirtual(int slot, std::uint32_t arg, bool hasArg) {
    std::ostringstream text;
    text << std::hex << std::nouppercase << "v" << slot;
    if (hasArg) {
        if (slot == 0x18 && arg >= 0x10000u) {
            auto* bytes = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(arg));
            if (bytes[0] == 1 && (bytes[1] == 1 || bytes[1] == 2)) {
                text << ":m" << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[0])
                     << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[1]);
                addLog(text.str());
                return 0;
            }
        }
        if (slot == 0x110 && g_fill110 && arg >= 0x10000u) {
            auto* out = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(arg));
            storeU32(out, g_out0);
            storeU32(out + 4, g_out1);
        }
        if (slot == 0x84 && arg >= 0x10000u) {
            auto* byte = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(arg));
            if (g_zero84) {
                *byte = 0;
            } else if (g_set84) {
                *byte = 1;
            }
        }
        text << argText(arg);
    }
    addLog(text.str());
    if (slot == 0x128) {
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_produced));
    }
    return popSlot(slot) ? popq() : 0;
}

#define V0(name, slot) \
    std::uint32_t __fastcall v0_##name(void*, void*) { return onVirtual(slot, 0, false); }
#define V1(name, slot) \
    std::uint32_t __fastcall v1_##name(void*, void*, std::uint32_t arg) { return onVirtual(slot, arg, true); }

V0(00, 0x00)
V1(00, 0x00)
V1(04, 0x04)
V0(08, 0x08)
V0(0C, 0x0C)
V1(0C, 0x0C)
V0(10, 0x10)
V1(18, 0x18)
V1(20, 0x20)
V1(58, 0x58)
V1(5C, 0x5C)
V1(60, 0x60)
V0(64, 0x64)
V1(64, 0x64)
V1(6C, 0x6C)
V1(70, 0x70)
V0(78, 0x78)
V1(7C, 0x7C)
V1(84, 0x84)
V0(AC, 0xAC)
V1(AC, 0xAC)
V0(B0, 0xB0)
V1(B0, 0xB0)
V1(10C, 0x10C)
V1(110, 0x110)
V1(128, 0x128)

#undef V0
#undef V1

#define D0(name, tag, pop) \
    std::uint32_t __fastcall d0_##name(void*, void*) { return onDirect(tag, nullptr, 0, pop); }
#define D1(name, tag, pop) \
    std::uint32_t __fastcall d1_##name(void*, void*, std::uint32_t a) { \
        const std::uint32_t args[] = {a}; \
        return onDirect(tag, args, 1, pop); \
    }
#define D2(name, tag, pop) \
    std::uint32_t __fastcall d2_##name(void*, void*, std::uint32_t a, std::uint32_t b) { \
        const std::uint32_t args[] = {a, b}; \
        return onDirect(tag, args, 2, pop); \
    }
#define D3(name, tag, pop) \
    std::uint32_t __fastcall d3_##name(void*, void*, std::uint32_t a, std::uint32_t b, std::uint32_t c) { \
        const std::uint32_t args[] = {a, b, c}; \
        return onDirect(tag, args, 3, pop); \
    }

D1(dock, "740690", false)
D1(open, "74f0e0", false)
D1(garr, "75d630", false)
D1(beh, "8d82f0", false)
D1(x10, "685080", false)
D1(upg, "6858e0", false)
D1(upd, "7321f0", false)
D1(dmg, "7302e0", false)
D1(del, "73f600", false)
D1(sup, "72e810", false)
D1(rs71, "71d5a0", false)
D1(rs73, "73a190", false)
D1(miss, "73aa60", false)
D0(att, "741d00", false)
D1(fwd, "752c00", false)
D1(cadd, "75df80", false)
D1(radd, "6de680", false)
D1(place, "78d1e0", false)
D1(slay, "691ab0", false)
D2(crem, "6b0db0", true)
D2(crem2, "6b0bd0", true)
D2(drem, "7704b0", false)
D1(spawn, "74c7a0", false)
D2(clr, "6d1ed0", false)
D3(build, "7483a0", false)
D1(techa, "755fa0", false)
D1(techb, "755e80", false)
D2(lpair, "75a8f0", false)
D0(lfin, "73b550", false)
D0(prep, "7653f0", false)
D0(mprep, "7432e0", false)
D1(has, "707050", true)
D2(make, "6fd820", true)
D2(grant, "71c890", false)
D2(loco, "718b80", false)
D2(mask, "6d2090", false)
D0(rew, "775ea0", false)
D0(bind, "6c74d0", false)
D1(link, "73a270", false)
D0(mark, "6a4fb0", false)
D0(falt, "701770", false)
D0(gbusy, "6b3200", true)
D2(gaim, "6a5900", true)
D0(gchk, "694000", true)
D0(gnear, "77ce10", true)
D0(gclr, "6941f0", false)
D0(gstep, "75eb00", false)
D1(son, "4d9ab0", false)
D1(soff, "4d9af0", false)
D1(pex, "757200", false)
D3(r45, "72dc90", false)
D3(r49, "72de40", false)
D3(r52, "72e080", false)
D3(i45, "6c5680", false)
D3(i49, "6c5710", false)
D3(i52, "6c57a0", false)

std::uint32_t __fastcall d0_target(void*, void*) {
    onDirect("6ebb60", nullptr, 0, false);
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_produced));
}
std::uint32_t __fastcall d0_state(void*, void*) {
    onDirect("5b9f30", nullptr, 0, false);
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_produced));
}
std::uint32_t __cdecl c_field(std::uint32_t a, std::uint32_t b) {
    const std::uint32_t args[] = {a, b};
    return onDirect("54a090", args, 2, false);
}
std::uint32_t __cdecl c_block(std::uint32_t a, std::uint32_t b) {
    const std::uint32_t args[] = {a, b};
    return onDirect("71d140", args, 2, false);
}
std::uint32_t __cdecl c_span(std::uint32_t a, std::uint32_t b) {
    const std::uint32_t args[] = {a, b};
    return onDirect("73a470", args, 2, false);
}
std::uint32_t __cdecl c_raw(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    const std::uint32_t args[] = {a, b, c};
    return onDirect("rawmove", args, 3, false);
}
std::uint32_t __cdecl c_stamp(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    const std::uint32_t args[] = {a, b, c};
    return onDirect("4ed9c0", args, 3, false);
}

#undef D0
#undef D1
#undef D2
#undef D3

struct Arena {
    std::vector<std::vector<std::uint8_t>> blocks;
    std::uint8_t* alloc(std::size_t size) {
        blocks.emplace_back(size, static_cast<std::uint8_t>(0x11));
        return blocks.back().data();
    }
};

void putPtr(std::uint8_t* object, int offset, const void* value) {
    storeU32(object + offset, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value)));
}

void putU32(std::uint8_t* object, int offset, std::uint32_t value) {
    storeU32(object + offset, value);
}

void putFn(std::uint8_t* vt, int slot, const void* fn) {
    putPtr(vt, slot, fn);
}

std::uint32_t parseU32(const std::string& token) {
    return static_cast<std::uint32_t>(std::strtoul(token.c_str(), nullptr, 0));
}

std::string hex32(std::uint32_t value) {
    std::ostringstream text;
    text << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << value;
    return text.str();
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

std::uint8_t* vtOf(Arena& arena, std::initializer_list<std::pair<int, const void*>> slots, int size = 0x140) {
    auto* vt = arena.alloc(static_cast<std::size_t>(size));
    for (const auto& slot : slots) {
        putFn(vt, slot.first, slot.second);
    }
    return vt;
}

std::uint8_t* xferWith(Arena& arena, std::uint32_t flags, std::initializer_list<std::pair<int, const void*>> extra) {
    auto* vt = vtOf(arena, {});
    putFn(vt, 0x18, &v1_18);
    for (const auto& slot : extra) {
        putFn(vt, slot.first, slot.second);
    }
    auto* xfer = arena.alloc(0x20);
    putPtr(xfer, 0, vt);
    putU32(xfer, 4, flags);
    return xfer;
}

void bindCalls() {
    using namespace ra3::gamelogic;
    setObject74Calls(Object74Calls{
        d1_dock, d1_open, d1_garr, d1_beh, d1_x10, d1_upg, d1_upd, d1_dmg, d1_del, d1_sup, d1_rs71, d1_rs73,
        d1_miss, d0_att, d0_target, d1_fwd, d1_cadd, d1_radd, d1_place, d1_slay, d2_crem, d2_crem2, d2_drem,
        d1_spawn, d2_clr, d0_state, d3_build, d1_techa, d1_techb, d2_lpair, d0_lfin, d0_prep, d0_mprep, d1_has, d2_make,
        d2_grant, d2_loco, d2_mask, d0_rew, d0_bind, d1_link, d0_mark, d0_falt, d0_gbusy, d2_gaim, d0_gchk,
        d0_gnear, d0_gclr, d0_gstep, d1_son, d1_soff, d1_pex, d3_r45, d3_r49, d3_r52, d3_i45, d3_i49, d3_i52,
        c_field, c_block, c_span, c_raw, c_stamp});
}

std::uint32_t bits(const void* address) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(address));
}

struct Reply {
    std::uint32_t eax = 0;
    std::string extra;
};

void emit(const Reply& reply, std::uint8_t* object, const std::vector<int>& offsets) {
    std::cout << "eax=" << hex32(reply.eax) << " log=" << (g_log.empty() ? "." : g_log)
              << " mem=" << memList(object, offsets);
    if (!reply.extra.empty()) {
        std::cout << " " << reply.extra;
    }
    std::cout << "\n";
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
        std::vector<std::uint32_t> queue;
        int phase = 0;
        std::string token;
        while (input >> token) {
            if (token == "q") {
                phase = 1;
                continue;
            }
            if (token == "mem") {
                phase = 2;
                continue;
            }
            if (phase == 2) {
                mem.push_back(static_cast<int>(parseU32(token)));
            } else if (phase == 1) {
                queue.push_back(parseU32(token));
            } else {
                args.push_back(token);
            }
        }
        auto need = [&](std::size_t n) { return args.size() >= n; };
        auto arg = [&](std::size_t index) { return parseU32(args[index]); };
        Arena arena;
        Reply reply;
        resetProbe();
        for (std::uint32_t value : queue) {
            if (g_qn < 16) {
                g_q[g_qn++] = value;
            }
        }
        auto* self = arena.alloc(0xA00);
        auto* clock = arena.alloc(0x80);
        putU32(clock, 0x58, need(1) && cmd == "clock" ? arg(0) : 0x3Cu);
        setObject74Env(Object74Env{clock, kScatter});
        bool known = true;

        if (cmd == "tcontain" && need(1)) {
            putU32(self, 0x148, arg(0));
            auto* xfer = xferWith(arena, 0, {{0x70, &v1_70}, {0x6C, &v1_6C}, {0x84, &v1_84}});
            reply.eax = TransportContain_xfer(self, xfer);
        } else if (cmd == "hadd" && need(1)) {
            auto* passenger = arena.alloc(0x20);
            auto* tmpl = arena.alloc(0x100);
            putU32(tmpl, 0xCC, arg(0));
            putPtr(passenger, 4, tmpl);
            auto* object = arena.alloc(0x40);
            putPtr(self, 8, object);
            reply.eax = HealContain_receivePassenger(self, passenger);
            const bool bit = (arg(0) & 0x400u) != 0;
            const bool ok = bit ? reply.eax == 0 : reply.eax == bits(tmpl);
            reply.extra = std::string("mix=") + (ok ? "1" : "0");
        } else if (cmd == "hgxfer") {
            auto* xfer = xferWith(arena, 0, {{0x6C, &v1_6C}});
            reply.eax = HordeGarrisonContain_xfer(self, xfer);
        } else if (cmd == "sadd" && need(1)) {
            auto* passenger = arena.alloc(0x20);
            auto* tmpl = arena.alloc(0x100);
            putU32(tmpl, 0xCC, arg(0));
            putPtr(passenger, 4, tmpl);
            auto* object = arena.alloc(0x40);
            putPtr(self, 8, object);
            reply.eax = SlaughterHordeContain_receivePassenger(self, passenger);
            const bool bit = (arg(0) & 0x400u) != 0;
            const bool ok = bit ? reply.eax == 0 : reply.eax == bits(tmpl);
            reply.extra = std::string("mix=") + (ok ? "1" : "0");
        } else if (cmd == "gsxfer") {
            auto* xfer = xferWith(arena, 0, {});
            reply.eax = GarrisonStealthContain_xfer(self, xfer);
        } else if (cmd == "hxfer") {
            auto* xfer = xferWith(arena, 0, {{0x6C, &v1_6C}});
            reply.eax = HealContain_xfer(self, xfer);
        } else if (cmd == "txfer" && need(1)) {
            auto* xfer = xferWith(arena, arg(0), {{0x6C, &v1_6C}, {0x84, &v1_84}});
            reply.eax = TunnelContain_xfer(self, xfer);
        } else if (cmd == "hrem") {
            auto* vt = vtOf(arena, {{0x78, &v0_78}});
            putPtr(self, 0, vt);
            auto* passenger = arena.alloc(0x10);
            reply.eax = HealContain_removePassenger(self, passenger);
        } else if (cmd == "srem") {
            auto* vt = vtOf(arena, {{0x78, &v0_78}});
            putPtr(self, 0, vt);
            auto* passenger = arena.alloc(0x10);
            reply.eax = SlaughterHordeContain_removePassenger(self, passenger);
        } else if (cmd == "rail") {
            auto* xfer = xferWith(arena, 0, {{0x64, &v1_64}, {0x70, &v1_70}});
            reply.eax = RailedTransportDockUpdate_xfer(self, xfer);
        } else if (cmd == "repair" && need(1)) {
            auto* xfer = xferWith(arena, arg(0), {{0x64, &v1_64}});
            reply.eax = RepairDockUpdate_xfer(self, xfer);
        } else if (cmd == "scenter") {
            auto* xfer = xferWith(arena, 0, {});
            reply.eax = SupplyCenterDockUpdate_xfer(self, xfer);
        } else if (cmd == "swh") {
            auto* xfer = xferWith(arena, 0, {{0x70, &v1_70}});
            reply.eax = SupplyWarehouseDockUpdate_xfer(self, xfer);
        } else if (cmd == "adep" && need(1)) {
            putU32(self, 0x24, 0x1000);
            putU32(self, 0x28, arg(0) == 2 ? 0x1008u : 0x1000u);
            auto* xfer = xferWith(arena, arg(0) == 1 ? 0x8u : (arg(0) == 2 ? 0x2u : 0u), {{0x6C, &v1_6C}, {0x84, &v1_84}});
            reply.eax = AutoDepositUpdate_xfer(self, xfer);
        } else if (cmd == "cluster" && need(1)) {
            putU32(self, 0x24, 0x11);
            putU32(self, 0x28, arg(0) == 3 ? 0u : 7u);
            putU32(self, 0x30, 0x2000);
            putU32(self, 0x34, arg(0) == 4 ? 0x2008u : 0x2000u);
            if (arg(0) == 1) {
                g_zero84 = true;
            }
            if (arg(0) == 3) {
                g_set84 = true;
            }
            auto* xfer = xferWith(arena, arg(0) == 4 ? 0x2u : 0u,
                                  {{0x04, &v1_04}, {0x08, &v0_08}, {0x0C, &v1_0C}, {0x20, &v1_20}, {0x84, &v1_84}});
            reply.eax = ClusterBombUpdate_xfer(self, xfer);
        } else if (cmd == "dmg") {
            auto* xfer = xferWith(arena, 0, {});
            reply.eax = DamageFieldUpdate_xfer(self, xfer);
        } else if (cmd == "struck" && need(2)) {
            auto* data = arena.alloc(0x10);
            putU32(data, 0, 0x41);
            putU32(data, 4, 0x42);
            putPtr(self, 0x54C, data);
            putPtr(self, 0x550, data + arg(1) * 4u);
            putU32(self, 0x520, 0x19);
            auto* xfer = xferWith(arena, arg(0), {{0x20, &v1_20}, {0x58, &v1_58}, {0x6C, &v1_6C}, {0x84, &v1_84}});
            reply.eax = SupplyTruckAIUpdate_xfer(self, xfer);
        } else if (cmd == "osxfer") {
            auto* xfer = xferWith(arena, 0, {{0x84, &v1_84}});
            reply.eax = ObjectStatusSpecialAbilityUpdate_xfer(self, xfer);
        } else if (cmd == "ocl" && need(2)) {
            auto* data = arena.alloc(0x10);
            putU32(data, 0, 1);
            putU32(data, 4, 2);
            putPtr(self, 0xB8, data);
            putPtr(self, 0xBC, data + arg(1) * 4u);
            auto* xfer = xferWith(arena, arg(0), {{0x6C, &v1_6C}});
            reply.eax = OCLMonitorSpecialAbilityUpdate_xfer(self, xfer);
        } else if (cmd == "repxfer" && need(1)) {
            auto* xfer = xferWith(arena, arg(0), {});
            reply.eax = ReplaceObjectUpdate_xfer(self, xfer);
        } else if (cmd == "subxfer" && need(2)) {
            auto* data = arena.alloc(0x10);
            putPtr(self, 0x30, data);
            putPtr(self, 0x34, data + arg(1) * 4u);
            auto* xfer = xferWith(arena, arg(0), {{0x7C, &v1_7C}, {0x84, &v1_84}});
            reply.eax = SubStructureBuildUpgrade_xfer(self, xfer);
        } else if (cmd == "delay") {
            auto* xfer = xferWith(arena, 0, {{0x6C, &v1_6C}});
            reply.eax = DelayedWeaponFireSpecialAbilityUpdate_xfer(self, xfer);
        } else if (cmd == "goal" && need(3)) {
            auto* coord = arena.alloc(0x10);
            putU32(coord, 0, arg(0));
            putU32(coord, 4, arg(1));
            putU32(coord, 8, arg(2));
            reply.eax = AIUpdateInterface_storeGoalPosition(self, coord);
        } else if (cmd == "missile" && need(2)) {
            putU32(clock, 0x58, arg(1));
            putU32(self, 0xE0, arg(0));
            putU32(self, 0xE4, 0x55);
            auto* body = arena.alloc(0x50);
            putU32(body, 0x38, 0x10);
            putU32(body, 0x3C, 0x20);
            putU32(body, 0x40, 0x30);
            putPtr(self, 8, body);
            reply.eax = MissileUpdate_arm(self, 0x6);
        } else if (cmd == "latch" && need(1)) {
            if (arg(0) == 0) {
                self[0x50] = 1;
                reply.eax = StealUnitUpdate_latch(self);
                reply.extra = std::string("mix=") + (reply.eax == 0 ? "1" : "0");
            } else if (arg(0) == 1) {
                g_produced = nullptr;
                reply.eax = StealUnitUpdate_latch(self);
                reply.extra = std::string("mix=") + (reply.eax == 0 ? "1" : "0");
            } else {
                auto* target = arena.alloc(0x430);
                auto* weaponHolder = arena.alloc(0x20);
                auto* weapon = arena.alloc(0x10);
                auto* weaponVt = vtOf(arena, {{0xB0, &v1_B0}});
                putPtr(weapon, 0, weaponVt);
                putPtr(weaponHolder, 0x14, weapon);
                putU32(target, 0x310, 1);
                putU32(target, 0x3C0, 1);
                putPtr(target, 0x3B4, weaponHolder);
                auto* record = arena.alloc(0x30);
                putU32(record, 0x20, 0x77);
                putPtr(target, 0x418, record);
                auto* object = arena.alloc(0x430);
                auto* orecord = arena.alloc(0x30);
                putU32(orecord, 0x20, 0x77);
                putPtr(object, 0x418, orecord);
                putPtr(self, 8, object);
                g_produced = target;
                reply.eax = StealUnitUpdate_latch(self);
                reply.extra = std::string("mix=") + (reply.eax == bits(record) ? "1" : "0");
            }
        } else if (cmd == "spawn" && need(1)) {
            auto* tmpl = arena.alloc(0x60);
            tmpl[0x58] = static_cast<std::uint8_t>(arg(0));
            putPtr(self, 4, tmpl);
            putU32(self, 0x6C, 0x2A);
            reply.eax = SpawnBehavior_prime(self, 0, 0);
            const bool ok = arg(0) == 0 ? reply.eax == bits(tmpl) : reply.eax == 0;
            reply.extra = std::string("mix=") + (ok ? "1" : "0");
        } else if (cmd == "mounted" && need(2)) {
            auto* object = self + 0x100;
            putPtr(self, 8, object);
            putU32(object, 0x88, arg(1));
            if (arg(0) != 0) {
                putPtr(object, 0xF4, self + 0x300);
            } else {
                putU32(object, 0xF4, 0);
            }
            reply.eax = ToggleMountedSpecialAbilityUpdate_clear(self);
        } else if (cmd == "dozer" && need(1)) {
            if (arg(0) == 0) {
                putU32(self, 0x5DC, 1);
                auto* vt = vtOf(arena, {{0x64, &v0_64}}, 0x80);
                putPtr(self, 0x51C, vt);
                reply.eax = DozerAIUpdate_routeBuild(self);
            } else if (arg(0) == 1) {
                putU32(self, 0x5DC, 0);
                auto* box = arena.alloc(0x10);
                putPtr(self, 0x538, box);
                g_produced = nullptr;
                reply.eax = DozerAIUpdate_routeBuild(self);
            } else {
                putU32(self, 0x5DC, 0);
                auto* box = arena.alloc(0x10);
                putPtr(self, 0x538, box);
                g_produced = arena.alloc(0x10);
                reply.eax = DozerAIUpdate_routeBuild(self);
            }
        } else if (cmd == "srelay" && need(1)) {
            if (arg(0) == 0) {
                g_produced = nullptr;
                reply.eax = StealUnitUpdate_relay(self, 0x9);
            } else {
                auto* target = arena.alloc(0x430);
                auto* weaponHolder = arena.alloc(0x20);
                auto* weapon = arena.alloc(0x10);
                putPtr(weapon, 0, vtOf(arena, {{0xB0, &v1_B0}}));
                putPtr(weaponHolder, 0x14, weapon);
                putU32(target, 0x310, 1);
                putU32(target, 0x3C0, 1);
                putPtr(target, 0x3B4, weaponHolder);
                auto* record = arena.alloc(0x30);
                putU32(record, 0x20, 0x21);
                putPtr(target, 0x418, record);
                g_produced = target;
                reply.eax = StealUnitUpdate_relay(self, 0x9);
            }
        } else if (cmd == "tech9") {
            putPtr(self, 0x14, vtOf(arena, {{0x00, &v0_00}}));
            auto* object = arena.alloc(0x430);
            putU32(object, 0x418, 0x42);
            putPtr(self, 0x0C, object);
            reply.eax = TechUpgrade_finish(self + 4);
        } else if (cmd == "tech10" && need(3)) {
            putPtr(self, 0x14, vtOf(arena, {{0x00, &v0_00}}));
            auto* object = arena.alloc(0x280);
            putU32(object, 0x26C, arg(0));
            putPtr(self, 0x0C, object);
            reply.eax = TechUpgrade_grant(self + 4, arg(1), arg(2));
        } else if (cmd == "laser" && need(3)) {
            putU32(clock, 0x58, 0x10);
            auto* object = arena.alloc(0x10);
            putPtr(self, 8, object);
            auto* tmpl = arena.alloc(0x30);
            putU32(tmpl, 0x14, arg(1));
            putU32(tmpl, 0x1C, arg(2));
            putPtr(self, 4, tmpl);
            reply.eax = LaserStateModule_schedule(self, arg(0), 0);
        } else if (cmd == "grant" && need(1)) {
            auto* object = arg(0) == 0 ? nullptr : arena.alloc(0x40);
            putPtr(self, 8, object);
            auto* tmpl = arena.alloc(0x100);
            putU32(tmpl, 0xF8, arg(0) == 1 ? 0u : 0x15u);
            putPtr(self, 4, tmpl);
            reply.eax = GiveOrRestoreUpgradeSpecialPower_apply(self);
            bool ok = false;
            if (arg(0) == 0) {
                ok = reply.eax == 0;
            } else if (arg(0) == 1) {
                ok = reply.eax == bits(tmpl);
            } else if (arg(0) == 2 || arg(0) == 3) {
                ok = reply.eax == 1;
            } else {
                ok = reply.eax == 0;
            }
            reply.extra = std::string("mix=") + (ok ? "1" : "0");
        } else if (cmd == "loco" && need(3)) {
            auto* object = self + 0x100;
            auto* tmpl = arena.alloc(0xD0);
            putPtr(self, 8, object);
            putPtr(self, 4, tmpl);
            putU32(object, 0x88, arg(1));
            for (int slot = 0; slot < 7; ++slot) {
                putU32(tmpl, 0xA8 + slot * 4, 0);
            }
            putU32(tmpl, 0xA8, arg(2));
            reply.eax = SwitchLocomotorsSpecialAbilityUpdate_commit(self, 0x3, arg(0));
        } else if (cmd == "deploy" && need(2)) {
            auto* object = arena.alloc(0xD0);
            putPtr(self, 8, object);
            putPtr(self, 0, vtOf(arena, {{0xAC, &v1_AC}, {0xB0, &v1_B0}}));
            putU32(self, 0x2C, arg(0));
            putU32(object, 0xC0, arg(1));
            reply.eax = ToggleDeploySpecialAbilityUpdate_flip(self);
            const bool ok = arg(0) != 1 ? reply.eax == bits(object) : reply.eax == 0;
            reply.extra = std::string("mix=") + (ok ? "1" : "0");
        } else if (cmd == "hidden" && need(2)) {
            auto* object = arena.alloc(0x90);
            putPtr(self, 8, object);
            putPtr(self, 0, vtOf(arena, {{0xAC, &v0_AC}, {0xB0, &v0_B0}}));
            putU32(self, 0x2C, arg(0));
            putU32(object, 0x80, arg(1));
            reply.eax = ToggleHiddenSpecialAbilityUpdate_flip(self);
        } else if (cmd == "tclear" && need(1)) {
            putU32(self, 0xBC, arg(0));
            reply.eax = ToggleStatusSpecialAbilityUpdate_rewind(self);
        } else if (cmd == "ostatus" && need(1)) {
            self[0xB8] = static_cast<std::uint8_t>(arg(0));
            auto* tmpl = arena.alloc(0x120);
            auto* object = arena.alloc(0x10);
            putPtr(self, 4, tmpl);
            putPtr(self, 8, object);
            reply.eax = ObjectStatusSpecialAbilityUpdate_swap(self);
        } else if (cmd == "mground" && need(1)) {
            auto* link = arena.alloc(0x30);
            auto* aim = arena.alloc(0x10);
            putPtr(self, 0x18, link);
            putPtr(link, 0x24, aim);
            g_produced = arg(0) == 1 ? nullptr : arena.alloc(0x10);
            reply.eax = MoveToGroundState_update(self);
        } else if ((cmd == "t45" || cmd == "t49" || cmd == "t52") && need(1)) {
            auto* object = arena.alloc(0x340);
            putPtr(self, 8, object);
            auto* contain = arena.alloc(0x20);
            putPtr(contain, 0, vtOf(arena, {{0xB0, &v0_B0}, {0x110, &v1_110}}));
            if (arg(0) == 0) {
                putU32(object, 0x328, 0);
                putU32(object, 0x32C, 0);
                putU32(object, 0x330, 0);
            } else if (arg(0) == 1) {
                auto* data = arena.alloc(0x10);
                putU32(data, 0, bits(contain));
                putU32(object, 0x328, 0);
                putPtr(object, 0x32C, data);
                putPtr(object, 0x330, data + 4);
            } else {
                putPtr(object, 0x328, contain);
            }
            auto* head = arena.alloc(0x10);
            auto* node = arena.alloc(0x10);
            auto* rider = arena.alloc(0x380);
            auto* module = arena.alloc(0x30);
            putPtr(head, 0, arg(0) == 4 ? head : node);
            putPtr(node, 0, head);
            putPtr(node, 8, rider);
            putPtr(rider, 0x374, arg(0) == 5 ? nullptr : module);
            auto* first = arena.alloc(0x10);
            putPtr(first, 0, vtOf(arena, {{0x00, &v1_00}}));
            g_fill110 = arg(0) >= 3;
            g_out1 = bits(head);
            g_out0 = arg(0) == 6 ? bits(first) : 0;
            const auto call = cmd == "t45"   ? TransportAIUpdate_notifyPassengers45
                              : cmd == "t49" ? TransportAIUpdate_notifyPassengers49
                                             : TransportAIUpdate_notifyPassengers52;
            reply.eax = call(self, 0x1, 0x2, arg(0) == 2 ? 2u : 0u);
        } else if (cmd == "ppow" && need(1)) {
            if (arg(0) == 0) {
                putU32(self, 8, 0);
            } else {
                auto* object = arena.alloc(0x430);
                putPtr(self, 8, object);
                if (arg(0) == 1) {
                    putU32(object, 0x418, 0);
                } else {
                    auto* record = arena.alloc(0x1330);
                    putPtr(object, 0x418, record);
                    if (arg(0) == 2) {
                        putU32(record, 0x1320, 0);
                    } else {
                        auto* power = arena.alloc(0x20);
                        putPtr(record, 0x1320, power);
                    }
                }
            }
            reply.eax = PlayerPowerManagerBehavior_refresh(self);
            bool ok = reply.eax == 0;
            if (arg(0) == 2) {
                ok = ok && loadU32(self + 0x40) == 0;
            } else if (arg(0) == 3) {
                auto* object = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(loadU32(self + 8)));
                auto* record = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(loadU32(object + 0x418)));
                auto* power = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(loadU32(record + 0x1320)));
                ok = ok && loadU32(self + 0x40) == bits(power);
            } else {
                ok = ok && loadU32(self + 0x40) == 0x11111111u;
            }
            reply.extra = std::string("mix=") + (ok ? "1" : "0");
        } else if (cmd == "fling" && need(1)) {
            auto* object = arena.alloc(0x340);
            auto* contain = arena.alloc(0x20);
            putPtr(self, 8, object);
            putPtr(object, 0x328, contain);
            putPtr(contain, 0, vtOf(arena, {{0x10C, &v1_10C}, {0x128, &v1_128}}));
            auto* unit = self + 0x200;
            g_produced = unit;
            unit[0x471] = arg(0) == 2 || arg(0) == 3 ? 0 : 1;
            unit[0xB2] = arg(0) == 1 ? 1 : 0;
            putU32(unit, 0xB0, 0);
            putU32(unit, 0xBC, arg(0) == 4 ? 0x80u : 0u);
            if (arg(0) == 2) {
                putPtr(unit, 0x378, arena.alloc(0x10));
            } else {
                putU32(unit, 0x378, 0);
            }
            reply.eax = FlingPassengerSpecialAbilityUpdate_fling(self);
            if (arg(0) == 3) {
                reply.extra = std::string("mix=") + (reply.eax == bits(unit) ? "1" : "0");
            }
        } else if (cmd == "scatter" && need(1)) {
            auto* face = self + 0x20;
            if (arg(0) == 0) {
                putU32(face, -8, 0);
            } else {
                auto* object = arena.alloc(0x350);
                auto* record = arena.alloc(0x30);
                putPtr(face, -8, object);
                putPtr(face, -12, record);
                record[0x29] = arg(0) == 1 ? 1 : 0;
                record[0x28] = arg(0) == 3 ? 1 : 0;
                putU32(record, 0x24, arg(0) == 3 ? 7u : 1u);
                if (arg(0) == 4) {
                    putU32(object, 0x33C, 0);
                } else {
                    auto* weapon = arena.alloc(0x10);
                    putPtr(weapon, 0, vtOf(arena, {{0x5C, &v1_5C}, {0x60, &v1_60}}));
                    putPtr(object, 0x33C, weapon);
                }
                auto* idBox = arena.alloc(0x8);
                putU32(idBox, 0, arg(0) == 5 ? 0xFFFFFFFFu : 0x22u);
                putPtr(record, 8, arg(0) == 6 ? nullptr : idBox);
            }
            reply.eax = WeaponScatterEffectUpdate_apply(face);
            self = face;
        } else if (cmd == "proj" && need(2)) {
            putU32(clock, 0x58, 0x8);
            auto* object = arena.alloc(0x50);
            putU32(object, 0x38, 0x1);
            putU32(object, 0x3C, 0x2);
            putU32(object, 0x40, 0x3);
            putPtr(self, 8, object);
            auto* shot = arena.alloc(0x10);
            putPtr(shot, 0, vtOf(arena, {{0x10, &v0_10}}));
            putPtr(self, 0x50, shot);
            auto* source = arena.alloc(0xE0);
            putU32(source, 0xDC, 0x44);
            putU32(source, 0x38, 0x5);
            putU32(source, 0x3C, 0x6);
            putU32(source, 0x40, 0x7);
            auto* coord = arena.alloc(0x10);
            putU32(coord, 0, 0xA);
            putU32(coord, 4, 0xB);
            putU32(coord, 8, 0xC);
            reply.eax = ProjectileUpdate_launch(self, arg(0) ? source : nullptr, coord, arg(1));
        } else if (cmd == "clock") {
            reply.eax = 0;
        } else {
            known = false;
        }
        if (!known) {
            std::cout << "eax=00000000 log=ERR mem=.\n";
            continue;
        }
        emit(reply, self, mem);
    }
    return 0;
}
