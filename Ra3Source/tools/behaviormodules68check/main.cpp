#include "ra3/gamelogic/BehaviorModules68.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string g_log;
int g_seq = 0;
bool g_clear = false;
std::uint32_t g_imm = 0;
std::uint32_t g_nret[0x400]{};

void resetProbe(bool clear, std::uint32_t imm) {
    g_log.clear();
    g_seq = 0;
    g_clear = clear;
    g_imm = imm;
    std::memset(g_nret, 0, sizeof(g_nret));
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

std::uint32_t onOne(int slot, void* arg) {
    const auto address = reinterpret_cast<std::uintptr_t>(arg);
    std::ostringstream text;
    text << std::hex << std::nouppercase;
    if (address < 0x10000u) {
        text << "i" << slot << ":" << address;
        addLog(text.str());
        return g_imm;
    }
    auto* bytes = static_cast<std::uint8_t*>(arg);
    if (bytes[0] == 1 && (bytes[1] == 1 || bytes[1] == 2)) {
        text << "m" << slot << ":" << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[0])
             << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[1]);
        addLog(text.str());
        if (g_clear && bytes[1] == 2) {
            bytes[1] = 0;
        }
        return 0;
    }
    ++g_seq;
    storeU32(arg, 0xA1000000u + static_cast<std::uint32_t>(g_seq));
    text << "f" << slot << ":" << std::dec << g_seq;
    addLog(text.str());
    return 0;
}

std::uint32_t onNone(int slot) {
    std::ostringstream text;
    text << std::hex << std::nouppercase << "n" << slot;
    addLog(text.str());
    return g_nret[slot];
}

void onThree(std::uint32_t first, void* field, std::uint32_t count) {
    ++g_seq;
    storeU32(field, 0xA1000000u + static_cast<std::uint32_t>(g_seq));
    std::ostringstream text;
    text << std::hex << std::nouppercase << "e88:" << first << ":" << count << ":" << std::dec << g_seq;
    addLog(text.str());
}

void stampHelper(void* self, const char* name) {
    ++g_seq;
    storeU32(static_cast<std::uint8_t*>(self) + 0x80, 0xD2000000u + static_cast<std::uint32_t>(g_seq));
    std::ostringstream text;
    text << name << ":" << std::dec << g_seq;
    addLog(text.str());
}

std::uint32_t __fastcall one04(void*, void*) { return onNone(0x04); }
std::uint32_t __fastcall one08(void*, void*, void* arg) { return onOne(0x08, arg); }
std::uint32_t __fastcall one10(void*, void*) { return onNone(0x10); }
std::uint32_t __fastcall one14(void*, void*) { return onNone(0x14); }
std::uint32_t __fastcall one18(void*, void*, void* arg) { return onOne(0x18, arg); }
std::uint32_t __fastcall one1c(void*, void*, std::uint32_t arg) {
    return onOne(0x1C, reinterpret_cast<void*>(static_cast<std::uintptr_t>(arg)));
}
std::uint32_t __fastcall one20(void*, void*, std::uint32_t arg) {
    return onOne(0x20, reinterpret_cast<void*>(static_cast<std::uintptr_t>(arg)));
}
std::uint32_t __fastcall none24(void*, void*) { return onNone(0x24); }
std::uint32_t __fastcall one24(void*, void*, std::uint32_t arg) {
    return onOne(0x24, reinterpret_cast<void*>(static_cast<std::uintptr_t>(arg)));
}
std::uint32_t __fastcall one28(void*, void*, std::uint32_t arg) {
    return onOne(0x28, reinterpret_cast<void*>(static_cast<std::uintptr_t>(arg)));
}
std::uint32_t __fastcall none2c(void*, void*) { return onNone(0x2C); }
std::uint32_t __fastcall none30(void*, void*) { return onNone(0x30); }
std::uint32_t __fastcall none34(void*, void*) { return onNone(0x34); }
std::uint32_t __fastcall one58(void*, void*, void* arg) { return onOne(0x58, arg); }
std::uint32_t __fastcall none58(void*, void*) { return onNone(0x58); }
std::uint32_t __fastcall none5c(void*, void*) { return onNone(0x5C); }
std::uint32_t __fastcall one60(void*, void*, void* arg) { return onOne(0x60, arg); }
std::uint32_t __fastcall none60(void*, void*) { return onNone(0x60); }
std::uint32_t __fastcall one64(void*, void*, void* arg) { return onOne(0x64, arg); }
std::uint32_t __fastcall one6c(void*, void*, std::uint32_t arg) {
    return onOne(0x6C, reinterpret_cast<void*>(static_cast<std::uintptr_t>(arg)));
}
std::uint32_t __fastcall ptr6c(void*, void*, void* arg) { return onOne(0x6C, arg); }
std::uint32_t __fastcall one70(void*, void*, void* arg) { return onOne(0x70, arg); }
std::uint32_t __fastcall one84(void*, void*, void* arg) { return onOne(0x84, arg); }
void __fastcall one88(void*, void*, std::uint32_t first, void* field, std::uint32_t count) {
    onThree(first, field, count);
}
std::uint32_t __fastcall none9c(void*, void*) { return onNone(0x9C); }
std::uint32_t __fastcall none274(void*, void*) { return onNone(0x274); }
std::uint32_t __fastcall none2a4(void*, void*) { return onNone(0x2A4); }

std::uint32_t __fastcall keyAt4(void* self, void*) {
    addLog("n10");
    return loadU32(static_cast<std::uint8_t*>(self) + 4);
}

void __fastcall help8d82(void* self, void*, void*) { stampHelper(self, "h8d82"); }
void __fastcall help8508(void* self, void*, void*) { stampHelper(self, "h8508"); }
void __fastcall help0850(void* self, void*, void*) { stampHelper(self, "h0850"); }
void __fastcall help2dde(void* self, void*, std::uint32_t first, std::uint32_t second) {
    storeU32(static_cast<std::uint8_t*>(self) + 0x80, first);
    storeU32(static_cast<std::uint8_t*>(self) + 0x84, second);
    std::ostringstream text;
    text << std::hex << std::nouppercase << "h2dde:" << first << ":" << second;
    addLog(text.str());
}
void __fastcall helpf8b(void* self, void*, std::uint32_t id) {
    storeU32(static_cast<std::uint8_t*>(self) + 0x80, id);
    std::ostringstream text;
    text << std::hex << std::nouppercase << "hf8b:" << id;
    addLog(text.str());
}
void __cdecl help4a09(void*, void* field) {
    ++g_seq;
    storeU32(field, 0xA1000000u + static_cast<std::uint32_t>(g_seq));
    std::ostringstream text;
    text << "c4a09:" << std::dec << g_seq;
    addLog(text.str());
}
void __cdecl help6340(void*, void* field) {
    ++g_seq;
    storeU32(field, 0xA1000000u + static_cast<std::uint32_t>(g_seq));
    std::ostringstream text;
    text << "c6340:" << std::dec << g_seq;
    addLog(text.str());
}

struct Arena {
    std::vector<std::vector<std::uint8_t>> blocks;
    std::uint8_t* alloc(std::size_t size) {
        blocks.emplace_back(size, static_cast<std::uint8_t>(0x11));
        return blocks.back().data();
    }
};

void putPtr(std::uint8_t* object, int offset, const void* value) {
    const auto bits = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));
    storeU32(object + offset, bits);
}

void putU32(std::uint8_t* object, int offset, std::uint32_t value) {
    storeU32(object + offset, value);
}

void putFn(std::uint8_t* vt, int slot, const void* fn) {
    putPtr(vt, slot, fn);
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

std::uint32_t parseU32(const std::string& token) {
    return static_cast<std::uint32_t>(std::strtoul(token.c_str(), nullptr, 0));
}

void fldcw(unsigned short control) {
    __asm { fldcw control }
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
    setBehavior68Calls(Behavior68Calls{help8d82, help8508, help0850, help2dde, helpf8b, help4a09, help6340});

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
        auto need = [&](std::size_t n) {
            return args.size() >= n;
        };
        Arena arena;
        Reply reply;
        std::uint8_t* self = arena.alloc(0x600);
        resetProbe(false, 0);

        if (cmd == "garrison" && need(2)) {
            auto* body = arena.alloc(0x100);
            body[0x9E] = static_cast<std::uint8_t>(parseU32(args[0]));
            putPtr(self, 4, body);
            putU32(self, 0x148, parseU32(args[1]));
            reply.eax = GarrisonContain_clearReservedIfOccupied(self, 0, 0);
            const auto bodyBits = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(body));
            reply.extra = std::string("mix=") + (reply.eax == bodyBits ? "1" : "0");
        } else if (cmd == "opencontain" && need(1)) {
            self[0x110] = static_cast<std::uint8_t>(parseU32(args[0]));
            OpenContain_clearClosingByte(self, 0, 0);
        } else if (cmd == "transport" && need(1)) {
            auto* body = arena.alloc(0x160);
            body[0x143] = static_cast<std::uint8_t>(parseU32(args[0]));
            putPtr(self, 4, body);
            reply.eax = TransportContain_passengerFlag(self, 0);
            const auto lo = reply.eax & 0xFFu;
            const auto expect = (static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(body)) & 0xFFFFFF00u) | lo;
            reply.extra = "lo=" + hex32(lo) + " mix=" + (reply.eax == expect ? "1" : "0");
        } else if (cmd == "idle" && need(1)) {
            auto* vt = arena.alloc(0x10);
            g_nret[0x04] = parseU32(args[0]);
            putFn(vt, 4, &one04);
            auto* second = arena.alloc(0x40);
            putPtr(second, 0x24, vt);
            auto* first = arena.alloc(0x90);
            putPtr(first, 0x80, second);
            putPtr(self, 0x18, first);
            reply.eax = IdleState_busyMask(self);
        } else if (cmd == "line" && need(1)) {
            g_clear = parseU32(args[0]) != 0;
            auto* vt = arena.alloc(0x80);
            putFn(vt, 0x18, &one18);
            putFn(vt, 0x58, &one58);
            putFn(vt, 0x64, &one64);
            auto* xfer = arena.alloc(0x20);
            putPtr(xfer, 0, vt);
            MoveInStraightLineState_xfer(self, xfer);
        } else if (cmd == "ground") {
            auto* vt = arena.alloc(0x80);
            putFn(vt, 0x18, &one18);
            putFn(vt, 0x64, &one64);
            auto* xfer = arena.alloc(0x20);
            putPtr(xfer, 0, vt);
            MoveToGroundState_xfer(self, xfer);
        } else if (cmd == "beam" && need(1)) {
            auto* body = arena.alloc(0x120);
            body[0x10D] = static_cast<std::uint8_t>(parseU32(args[0]));
            putPtr(self, 4, body);
            reply.eax = BeamSpecialAbilityUpdate_templateFlag(self);
            const auto lo = reply.eax & 0xFFu;
            const auto expect = (static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(body)) & 0xFFFFFF00u) | lo;
            reply.extra = "lo=" + hex32(lo) + " mix=" + (reply.eax == expect ? "1" : "0");
        } else if (cmd == "radius" && need(1)) {
            auto* body = arena.alloc(0x100);
            putU32(body, 0xF0, parseU32(args[0]));
            putPtr(self, 4, body);
            std::uint8_t out[10]{};
            fldcw(0x037F);
            DominateEnemySpecialPower_storeRadius80(self, out);
            std::ostringstream text;
            text << "st0=";
            for (unsigned char byte : out) {
                text << std::hex << std::nouppercase << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
            }
            reply.extra = text.str();
        } else if (cmd == "toggle" && need(2)) {
            auto* table = arena.alloc(0x2000);
            auto* body = arena.alloc(0x100);
            putU32(body, 0xF0, parseU32(args[1]));
            putPtr(body, 0xF4, table);
            putPtr(self, 4, body);
            putU32(self, 0xBC, parseU32(args[0]));
            const std::uint32_t which = args.size() > 2 ? parseU32(args[2]) : 0;
            const std::uint32_t result = which == 0
                ? ToggleStatusSpecialAbilityUpdate_entryPointer(self)
                : ToggleStatusSpecialAbilityUpdate_nextEntryPointer(self);
            const auto base = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(table));
            reply.extra = "off=" + hex32(result - base);
        } else if (cmd == "dozer") {
            auto* vt = arena.alloc(0x90);
            putFn(vt, 0x18, &one18);
            putFn(vt, 0x6C, &ptr6c);
            putFn(vt, 0x70, &one70);
            putFn(vt, 0x84, &one84);
            auto* xfer = arena.alloc(0x20);
            putPtr(xfer, 0, vt);
            DozerPrimaryIdleState_xfer(self, xfer);
        } else if (cmd == "dock" && need(3)) {
            auto* body = arena.alloc(0x40);
            putU32(body, 0x1C, parseU32(args[1]));
            putPtr(self, 4, body);
            putU32(self, 0x8C, parseU32(args[2]));
            auto* update = arena.alloc(0x100);
            std::uint8_t* dock = nullptr;
            if (parseU32(args[0]) != 0) {
                dock = arena.alloc(0x100);
                putPtr(update, 0xF4, dock);
            } else {
                putU32(update, 0xF4, 0);
            }
            putPtr(self, 8, update);
            SupplyWarehouseDockUpdate_dispatch(self);
            if (dock != nullptr) {
                reply.extra = "d80=" + hex32(loadU32(dock + 0x80)) + " d84=" + hex32(loadU32(dock + 0x84));
            } else {
                reply.extra = "d80=00000000 d84=00000000";
            }
        } else if (cmd == "ensure" && need(2)) {
            auto* vt = arena.alloc(0x2C0);
            putFn(vt, 0x2A4, &none2a4);
            putPtr(self, 0, vt);
            if (parseU32(args[0]) != 0) {
                putU32(self, 0x3C, parseU32(args[1]));
                reply.eax = AIUpdateInterface_ensureLocomotor(self);
                reply.extra = "stored=0";
            } else {
                auto* locoVt = arena.alloc(0x20);
                putFn(locoVt, 0x14, &one14);
                auto* loco = arena.alloc(0x20);
                putPtr(loco, 0, locoVt);
                g_nret[0x2A4] = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(loco));
                g_nret[0x14] = parseU32(args[1]);
                putU32(self, 0x3C, 0);
                reply.eax = AIUpdateInterface_ensureLocomotor(self);
                const bool stored = loadU32(self + 0x3C) == g_nret[0x2A4];
                reply.extra = std::string("stored=") + (stored ? "1" : "0");
            }
        } else if (cmd == "setgoal" && need(2)) {
            const std::uint32_t value = parseU32(args[1]);
            if (parseU32(args[0]) == 0) {
                reply.eax = AIUpdateInterface_setGoal(self, value);
            } else {
                reply.eax = AIUpdateInterface_setGoalAux(self, value);
            }
        } else if (cmd == "query34" && need(2)) {
            const std::uint32_t mode = parseU32(args[0]);
            auto* loco = arena.alloc(0x20);
            if (mode == 0) {
                putU32(self, 0x3C, 0);
                reply.eax = AIUpdateInterface_query34(self);
                reply.extra = "lo=00000000 mix=1";
            } else if (mode == 1) {
                putU32(loco, 4, 0);
                putPtr(self, 0x3C, loco);
                reply.eax = AIUpdateInterface_query34(self);
                const auto lo = reply.eax & 0xFFu;
                const auto expect = (static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(loco)) & 0xFFFFFF00u) | lo;
                reply.extra = "lo=" + hex32(lo) + " mix=" + (reply.eax == expect ? "1" : "0");
            } else {
                auto* innerVt = arena.alloc(0x40);
                putFn(innerVt, 0x34, &none34);
                auto* inner = arena.alloc(0x20);
                putPtr(inner, 0, innerVt);
                putPtr(loco, 4, inner);
                putPtr(self, 0x3C, loco);
                g_nret[0x34] = parseU32(args[1]);
                reply.eax = AIUpdateInterface_query34(self);
            }
        } else if (cmd == "advance" && need(3)) {
            auto* vt = arena.alloc(0x280);
            putFn(vt, 0x274, &none274);
            putPtr(self, 0, vt);
            g_nret[0x274] = parseU32(args[0]);
            putU32(self, 0x188, parseU32(args[1]));
            if (parseU32(args[2]) != 0) {
                auto* leaf = arena.alloc(0x80);
                putU32(leaf, 0x64, parseU32(args[3]));
                auto* mid = arena.alloc(0x10);
                putPtr(mid, 4, leaf);
                auto* link = arena.alloc(0x10);
                putPtr(link, 4, mid);
                putPtr(self, 0x214, link);
            } else {
                putU32(self, 0x214, 0);
            }
            reply.eax = AIUpdateInterface_wantsAdvance(self);
        } else if ((cmd == "query24" || cmd == "query2c" || cmd == "query30") && need(2)) {
            const int slot = cmd == "query24" ? 0x24 : cmd == "query2c" ? 0x2C : 0x30;
            auto* loco = arena.alloc(0x20);
            putPtr(self, 0x3C, loco);
            if (parseU32(args[0]) == 0) {
                putU32(loco, 4, 0);
                reply.eax = cmd == "query24" ? AIUpdateInterface_query24(self)
                            : cmd == "query2c" ? AIUpdateInterface_query2C(self)
                                               : AIUpdateInterface_query30(self);
                const auto lo = reply.eax & 0xFFu;
                const auto expect = (static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(loco)) & 0xFFFFFF00u) | lo;
                reply.extra = "lo=" + hex32(lo) + " mix=" + (reply.eax == expect ? "1" : "0");
            } else {
                auto* innerVt = arena.alloc(0x40);
                putFn(innerVt, slot, slot == 0x24 ? &none24 : slot == 0x2C ? &none2c : &none30);
                auto* inner = arena.alloc(0x10);
                putPtr(inner, 0, innerVt);
                putPtr(loco, 4, inner);
                g_nret[slot] = parseU32(args[1]);
                reply.eax = cmd == "query24" ? AIUpdateInterface_query24(self)
                            : cmd == "query2c" ? AIUpdateInterface_query2C(self)
                                               : AIUpdateInterface_query30(self);
            }
        } else if (cmd == "state4d" && need(1)) {
            auto* vt = arena.alloc(0x40);
            putFn(vt, 0x34, &none34);
            putFn(vt, 0x1C, &one1c);
            auto* loco = arena.alloc(0x10);
            putPtr(loco, 0, vt);
            putPtr(self, 0x3C, loco);
            const std::uint32_t arg = parseU32(args[0]);
            reply.eax = AIUpdateInterface_enterState4D(self, arg);
        } else if (cmd == "forward" && need(4)) {
            const int slot = parseU32(args[0]) == 0 ? 0x20 : 0x24;
            if (parseU32(args[1]) == 0) {
                putU32(self, 0x3C, 0);
                reply.eax = slot == 0x20 ? AIUpdateInterface_forwardOrZero20(self, parseU32(args[2]))
                                         : AIUpdateInterface_forwardOrZero24(self, parseU32(args[2]));
            } else {
                auto* vt = arena.alloc(0x30);
                putFn(vt, slot, slot == 0x20 ? &one20 : &one24);
                auto* loco = arena.alloc(0x10);
                putPtr(loco, 0, vt);
                putPtr(self, 0x3C, loco);
                g_imm = parseU32(args[3]);
                reply.eax = slot == 0x20 ? AIUpdateInterface_forwardOrZero20(self, parseU32(args[2]))
                                         : AIUpdateInterface_forwardOrZero24(self, parseU32(args[2]));
            }
        } else if (cmd == "leech" && need(3)) {
            auto* object = arena.alloc(0x340);
            if (parseU32(args[0]) == 0) {
                putU32(object, 0x33C, 0);
                reply.eax = LeechTargetingAttachUpdate_notify(self, object);
            } else {
                auto* vt = arena.alloc(0x10);
                putFn(vt, 8, &one08);
                auto* listener = arena.alloc(0x10);
                putPtr(listener, 0, vt);
                putPtr(object, 0x33C, listener);
                auto* draw = arena.alloc(0xE0);
                putU32(draw, 0xDC, parseU32(args[1]));
                putPtr(self, 8, draw);
                g_imm = parseU32(args[2]);
                reply.eax = LeechTargetingAttachUpdate_notify(self, object);
            }
        } else if (cmd == "missile" && need(1)) {
            putU32(self, 0x11C, parseU32(args[0]));
            MissileUpdate_clearTimer(self);
        } else if (cmd == "projectile" && need(4)) {
            auto* clock = arena.alloc(0x1D0);
            putU32(clock, 0x1C4, parseU32(args[1]));
            putU32(clock, 0x58, parseU32(args[3]));
            float frames = 0.f;
            float scale = 0.f;
            const std::uint32_t frameBits = parseU32(args[0]);
            const std::uint32_t scaleBits = parseU32(args[2]);
            std::memcpy(&frames, &frameBits, 4);
            std::memcpy(&scale, &scaleBits, 4);
            fldcw(0x037F);
            reply.eax = ProjectileUpdate_addFrames(self, frames, clock, scale);
        } else if (cmd == "repair" && need(2)) {
            putU32(self, 0x30, parseU32(args[1]));
            if (parseU32(args[0]) == 0) {
                reply.eax = RepairUpdate_notify(self, nullptr);
                reply.extra = "p80=00000000";
            } else {
                auto* vt = arena.alloc(0x80);
                putFn(vt, 0x6C, &one6c);
                auto* player = arena.alloc(0x90);
                putPtr(player, 0, vt);
                g_imm = args.size() > 2 ? parseU32(args[2]) : 0;
                reply.eax = RepairUpdate_notify(self, player);
                reply.extra = "p80=" + hex32(loadU32(player + 0x80));
            }
        } else if (cmd == "countdown" && need(2)) {
            auto* vt = arena.alloc(0xA0);
            putFn(vt, 0x9C, &none9c);
            putPtr(self, 0, vt);
            putU32(self, 0x3C, parseU32(args[0]));
            g_nret[0x9C] = parseU32(args[1]);
            reply.eax = SpecialAbilityUpdate_countdown(self);
        } else if (cmd == "relay" && need(2)) {
            auto* base = arena.alloc(0x40);
            self = base + 0x10;
            auto* vt = arena.alloc(0x60);
            putFn(vt, 0x5C, &none5c);
            putPtr(base, 0, vt);
            self[4] = static_cast<std::uint8_t>(parseU32(args[0]));
            g_nret[0x5C] = parseU32(args[1]);
            reply.eax = EncloseBehavior_relayIfArmed(self);
        } else if (cmd == "tryopen" && need(1)) {
            auto* base = arena.alloc(0x40);
            self = base + 0x10;
            auto* baseVt = arena.alloc(0x70);
            putFn(baseVt, 0x58, &none58);
            putFn(baseVt, 0x60, &none60);
            putFn(baseVt, 0x5C, &none5c);
            putPtr(base, 0, baseVt);
            auto* faceVt = arena.alloc(0x30);
            putFn(faceVt, 8, &one08);
            putFn(faceVt, 0x28, &one28);
            putPtr(self, 0, faceVt);
            g_imm = parseU32(args[0]);
            reply.eax = EncloseBehavior_tryOpen(self, 0);
        } else if (cmd == "find" && need(1)) {
            const std::uint32_t count = parseU32(args[0]);
            auto* list = arena.alloc((count + 1) * 4);
            auto* keyVt = arena.alloc(0x14);
            putFn(keyVt, 0x10, &keyAt4);
            for (std::uint32_t i = 0; i < count; ++i) {
                auto* module = arena.alloc(0x10);
                putPtr(module, 0, keyVt);
                putU32(module, 4, parseU32(args[1 + i]));
                putPtr(list, static_cast<int>(i * 4), module);
            }
            putU32(list, static_cast<int>(count * 4), 0);
            putPtr(self, 0x310, list);
            const std::uint32_t found = findModuleByInterfaceKey(self, parseU32(args[1 + count]));
            int index = -1;
            for (std::uint32_t i = 0; i < count; ++i) {
                if (found == loadU32(list + i * 4)) {
                    index = static_cast<int>(i);
                }
            }
            reply.extra = "idx=" + std::to_string(index);
        } else if (cmd == "coord" && need(3)) {
            std::uint8_t coord[12]{};
            putU32(coord, 0, parseU32(args[0]));
            putU32(coord, 4, parseU32(args[1]));
            putU32(coord, 8, parseU32(args[2]));
            reply.eax = storeCoordAndSentinelSign(self, coord);
        } else if (cmd == "areaxfer") {
            auto* vt = arena.alloc(0x20);
            putFn(vt, 0x18, &one18);
            auto* xfer = arena.alloc(0x10);
            putPtr(xfer, 0, vt);
            AreaRestrictSpecialPowerBehavior_xfer(self, xfer);
        } else if (cmd == "supplyxfer") {
            auto* vt = arena.alloc(0x20);
            putFn(vt, 0x18, &one18);
            auto* xfer = arena.alloc(0x10);
            putPtr(xfer, 0, vt);
            SupplyCenterCreate_xfer(self, xfer);
        } else if (cmd == "autoxfer") {
            auto* vt = arena.alloc(0x90);
            putFn(vt, 0x18, &one18);
            putFn(vt, 0x60, &one60);
            putFn(vt, 0x84, &one84);
            putFn(vt, 0x58, &one58);
            auto* xfer = arena.alloc(0x10);
            putPtr(xfer, 0, vt);
            AutoAbilityBehavior_xfer(self, xfer);
        } else if (cmd == "bridge" && need(5)) {
            auto* ends = arena.alloc(0x40);
            auto* info = arena.alloc(0x30);
            putU32(self, 0x28, 0);
            putU32(self, 0x2C, 0);
            putU32(ends, 0x38, parseU32(args[0]));
            putU32(ends, 0x3C, parseU32(args[1]));
            putU32(info, 0x10, parseU32(args[2]));
            putU32(info, 0x18, parseU32(args[3]));
            if (parseU32(args[4]) != 0) {
                auto* alt = arena.alloc(0x10);
                putU32(alt, 0, parseU32(args[5]));
                putPtr(info, 0x20, alt);
                putU32(info, 0x1C, 1);
            } else {
                putU32(info, 0x1C, 0);
                putU32(info, 0x20, 0);
            }
            putPtr(self, 8, ends);
            putPtr(self, 4, info);
            BridgeBuilderBehavior_countSpanSegments(self);
        } else if (cmd == "moneyxfer") {
            auto* vt = arena.alloc(0x90);
            putFn(vt, 0x18, &one18);
            putFn(vt, 0x84, &one84);
            auto* xfer = arena.alloc(0x10);
            putPtr(xfer, 0, vt);
            DistributedMoneyBehavior_xfer(self, xfer);
        } else if (cmd == "energy" && need(4)) {
            std::uint8_t* target = nullptr;
            if (parseU32(args[0]) == 0) {
                putU32(self, 8, 0);
            } else {
                auto* object = arena.alloc(0x420);
                putU32(object, 0xDC, parseU32(args[3]));
                if (parseU32(args[1]) == 0) {
                    putU32(object, 0x418, 0);
                } else {
                    auto* player = arena.alloc(0x80);
                    if (parseU32(args[2]) == 0) {
                        putU32(player, 0x74, 0);
                    } else {
                        target = arena.alloc(0x90);
                        putPtr(player, 0x74, target);
                    }
                    putPtr(object, 0x418, player);
                }
                putPtr(self, 8, object);
            }
            EnergyDisablingBehavior_notifyOwner(self);
            reply.extra = "t80=" + hex32(target != nullptr ? loadU32(target + 0x80) : 0);
        } else if (cmd == "copy" && need(11)) {
            auto* src = arena.alloc(0x30);
            for (int i = 0; i < 11; ++i) {
                putU32(src, i * 4, parseU32(args[static_cast<std::size_t>(i)]));
            }
            auto* dst = copyPackedDamageBlock(self, src);
            reply.extra = std::string("retdst=") + (dst == self ? "1" : "0");
            for (int i = 0; i < 11; ++i) {
                mem.push_back(i * 4);
            }
        } else if (cmd == "sum" && need(1)) {
            const std::uint32_t count = parseU32(args[0]);
            auto* pairs = arena.alloc((count + 1) * 8);
            for (std::uint32_t i = 0; i < count; ++i) {
                putU32(pairs, static_cast<int>(i * 8), parseU32(args[1 + i]));
            }
            putPtr(self, 0x24, pairs);
            putPtr(self, 0x28, pairs + count * 8);
            reply.eax = sumStride8Keys(self);
        } else if (cmd == "power" && need(1)) {
            g_clear = parseU32(args[0]) != 0;
            auto* vt = arena.alloc(0x90);
            putFn(vt, 0x18, &one18);
            putFn(vt, 0x84, &one84);
            auto* xfer = arena.alloc(0x10);
            putPtr(xfer, 0, vt);
            PlayerPowerManagerBehavior_xfer(self, xfer);
        } else if (cmd == "slave") {
            auto* vt = arena.alloc(0x20);
            putFn(vt, 0x18, &one18);
            auto* xfer = arena.alloc(0x10);
            putPtr(xfer, 0, vt);
            SlaveWatcherBehavior_xfer(self, xfer);
        } else if (cmd == "stances") {
            auto* vt = arena.alloc(0x90);
            putFn(vt, 0x18, &one18);
            putFn(vt, 0x88, reinterpret_cast<void*>(&one88));
            auto* xfer = arena.alloc(0x10);
            putPtr(xfer, 0, vt);
            StancesBehavior_xfer(self, xfer);
        } else {
            std::cout << "err=unknown\n";
            continue;
        }
        emit(reply, self, mem);
    }
    return 0;
}
