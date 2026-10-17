#include "ra3/gamelogic/ObjectModules70.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr std::uint32_t kBase = 0x60000000u;
constexpr std::uint32_t kSelf = kBase + 0x200u;
constexpr std::uint32_t kP = kSelf + 0x400u;
constexpr std::uint32_t kQ = kSelf + 0x600u;
constexpr std::uint32_t kR = kSelf + 0x800u;
constexpr std::uint32_t kS = kSelf + 0xA00u;
constexpr std::uint32_t kT = kSelf + 0xC00u;
constexpr std::uint32_t kU = kSelf + 0xE00u;
constexpr std::uint32_t kVec = kBase + 0x1C00u;
constexpr std::uint32_t kXfer = kBase + 0x2800u;
constexpr std::uint32_t kNode = kBase + 0x2C00u;
constexpr std::uint32_t kVt = kBase + 0x3000u;
constexpr std::uint32_t kVt2 = kBase + 0x3200u;
constexpr std::uint32_t kVt3 = kBase + 0x3400u;
constexpr std::uint32_t kVt4 = kBase + 0x3600u;

std::string g_log;
std::vector<std::uint32_t> g_q;
int g_qi = 0;
int g_forceMinor = -1;
bool g_nudge = false;
bool g_touch34 = false;

const std::uint32_t kKnown[] = {
    0x00C053B8u, 0x00BC8E74u, 0x00BC8E80u, 0x00711ED0u, 0x7FFFFFFFu,
    0x3F800000u, 0x40000000u, 0x3F000000u, 0xBF800000u, 0xC0000000u,
    0x40400000u, 0x40800000u, 0x41200000u, 0x7FC00000u, 0x80000000u,
    0x3FC00000u, 0x40A00000u};

void addLog(const std::string& text) {
    if (!g_log.empty()) {
        g_log += ",";
    }
    g_log += text;
}

std::uint32_t ld(std::uint32_t address) {
    std::uint32_t value = 0;
    std::memcpy(&value, reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)), 4);
    return value;
}

void st(std::uint32_t address, std::uint32_t value) {
    std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)), &value, 4);
}

void sb(std::uint32_t address, std::uint8_t value) {
    *reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(address)) = value;
}

std::uint32_t popq() {
    if (g_qi >= static_cast<int>(g_q.size())) {
        return 0;
    }
    return g_q[g_qi++];
}

bool known(std::uint32_t arg) {
    for (std::uint32_t item : kKnown) {
        if (arg == item) {
            return true;
        }
    }
    return false;
}

std::string argText(std::uint32_t arg) {
    std::ostringstream text;
    text << std::hex << std::nouppercase;
    const bool arena = arg >= kBase && arg < kBase + 0x4000u;
    if (arg < 0x10000u || arena || known(arg) || arg >= 0xFFFF0000u) {
        text << ":" << arg;
    } else {
        text << ":s";
    }
    return text.str();
}

void logDirect(const char* tag, const std::uint32_t* args, int count) {
    std::ostringstream text;
    text << "c" << tag;
    for (int i = 0; i < count; ++i) {
        text << argText(args[i]);
    }
    addLog(text.str());
}

std::uint32_t onVirtual(std::uint32_t slot, const std::uint32_t* args, int count) {
    std::ostringstream text;
    text << std::hex << std::nouppercase << "v" << slot;
    if (slot == 0x18 && count == 1 && args[0] >= 0x10000u) {
        auto* bytes = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(args[0]));
        text << ":" << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[0])
             << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[1]);
        addLog(text.str());
        if (g_forceMinor >= 0) {
            bytes[1] = static_cast<std::uint8_t>(g_forceMinor);
        }
        return popq();
    }
    for (int i = 0; i < count; ++i) {
        text << argText(args[i]);
    }
    addLog(text.str());
    return popq();
}

#define V0(name, slot) \
    std::uint32_t __fastcall v0_##name(void*, void*) { return onVirtual(slot, nullptr, 0); }
#define V1(name, slot) \
    std::uint32_t __fastcall v1_##name(void*, void*, std::uint32_t a) { \
        const std::uint32_t args[] = {a}; \
        return onVirtual(slot, args, 1); \
    }
#define V2(name, slot) \
    std::uint32_t __fastcall v2_##name(void*, void*, std::uint32_t a, std::uint32_t b) { \
        const std::uint32_t args[] = {a, b}; \
        return onVirtual(slot, args, 2); \
    }

V1(08, 0x08)
V0(14, 0x14)
V1(18, 0x18)
V1(2c, 0x2c)
V2(58, 0x58)
V0(60, 0x60)
V1(64, 0x64)
V1(6c1, 0x6c)
V1(70, 0x70)
V1(84, 0x84)
V0(ac, 0xac)
V1(b4, 0xb4)
V0(f4, 0xf4)

#undef V0
#undef V1
#undef V2

std::uint32_t __fastcall v0_6c0(void* self, void*) {
    if (g_touch34) {
        const auto field = reinterpret_cast<std::uint32_t>(self) + 0x44;
        st(field, ld(field) + 1);
    }
    return onVirtual(0x6c, nullptr, 0);
}

#define D0(name, tag) \
    std::uint32_t __fastcall d0_##name(void*, void*) { \
        logDirect(tag, nullptr, 0); \
        return popq(); \
    }
#define D1(name, tag) \
    std::uint32_t __fastcall d1_##name(void*, void*, std::uint32_t a) { \
        const std::uint32_t args[] = {a}; \
        logDirect(tag, args, 1); \
        return popq(); \
    }
#define D2(name, tag) \
    std::uint32_t __fastcall d2_##name(void*, void*, std::uint32_t a, std::uint32_t b) { \
        const std::uint32_t args[] = {a, b}; \
        logDirect(tag, args, 2); \
        return popq(); \
    }
#define D3(name, tag) \
    std::uint32_t __fastcall d3_##name(void*, void*, std::uint32_t a, std::uint32_t b, std::uint32_t c) { \
        const std::uint32_t args[] = {a, b, c}; \
        logDirect(tag, args, 3); \
        return popq(); \
    }
#define D4(name, tag) \
    std::uint32_t __fastcall d4_##name(void*, void*, std::uint32_t a, std::uint32_t b, std::uint32_t c, \
                                       std::uint32_t d) { \
        const std::uint32_t args[] = {a, b, c, d}; \
        logDirect(tag, args, 4); \
        return popq(); \
    }
#define D5(name, tag) \
    std::uint32_t __fastcall d5_##name(void*, void*, std::uint32_t a, std::uint32_t b, std::uint32_t c, \
                                       std::uint32_t d, std::uint32_t e) { \
        const std::uint32_t args[] = {a, b, c, d, e}; \
        logDirect(tag, args, 5); \
        return popq(); \
    }
#define D6(name, tag) \
    std::uint32_t __fastcall d6_##name(void*, void*, std::uint32_t a, std::uint32_t b, std::uint32_t c, \
                                       std::uint32_t d, std::uint32_t e, std::uint32_t f) { \
        const std::uint32_t args[] = {a, b, c, d, e, f}; \
        logDirect(tag, args, 6); \
        return popq(); \
    }
#define C1(name, tag) \
    std::uint32_t __cdecl c1_##name(std::uint32_t a) { \
        const std::uint32_t args[] = {a}; \
        logDirect(tag, args, 1); \
        return popq(); \
    }
#define C2(name, tag) \
    std::uint32_t __cdecl c2_##name(std::uint32_t a, std::uint32_t b) { \
        const std::uint32_t args[] = {a, b}; \
        logDirect(tag, args, 2); \
        return popq(); \
    }
#define C3(name, tag) \
    std::uint32_t __cdecl c3_##name(std::uint32_t a, std::uint32_t b, std::uint32_t c) { \
        const std::uint32_t args[] = {a, b, c}; \
        logDirect(tag, args, 3); \
        return popq(); \
    }
#define C6(name, tag) \
    std::uint32_t __cdecl c6_##name(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d, \
                                    std::uint32_t e, std::uint32_t f) { \
        const std::uint32_t args[] = {a, b, c, d, e, f}; \
        logDirect(tag, args, 6); \
        return popq(); \
    }

C1(free, "4166b0")
C6(node, "416860")
D0(item, "4da760")
D1(phas, "4e2140")
D2(rpair, "4e41b0")
D0(rone, "4ed380")
D2(pcnt, "4f6850")
D4(seed, "506c20")
D2(fin, "52efc0")
D0(pready, "538530")
C2(xblock, "539df0")
D1(xnode, "683e70")
D1(x10, "685080")
D1(find, "687460")
D0(bcnt, "68b180")
D0(lprep, "69d000")
D2(ucheck, "69fa90")
D2(uapply, "6fd820")
D0(eff, "6a56a0")
D0(ammo, "6a8400")
D1(enter, "6bcf30")
D2(give, "6c1c50")
D2(scan, "7ed750")
D0(pulse, "6c8560")
D3(recall, "6cace0")
D2(status, "6d1ed0")
D1(sflag, "6d23f0")
D0(prone, "6e96e0")
D6(ocl, "6ea540")
D3(point, "6ea9d0")
D1(sub, "6ece70")
D5(fire, "6ee8f0")
D1(slot, "6f0a20")
D0(wquery, "6f2050")
D2(wlink, "6f2730")
D2(latt, "6f42d0")
D2(spush, "6f8b80")
D0(horde, "6fbbd0")
D0(ready, "702070")
D1(door, "707050")
C2(xt30, "708190")
C2(xt2c, "7087d0")
D0(stop, "700e50")
D0(ocld, "7010d0")
D0(cdmg, "701330")
D0(sprobe, "7174c0")
D0(salt, "7176b0")
D0(smain, "717b40")
C2(field, "71d020")
D2(bezier, "71f0f0")
D1(dalt, "7f3120")
D1(dscan, "78da40")
D1(gfind, "71abe0")
C2(xpair, "54a090")
D1(bplace, "860630")
D1(xbase, "8d82f0")
C3(range, "9279f0")
D1(nkey, "986150")
C3(bytes, "bba500")

#undef D0
#undef D1
#undef D2
#undef D3
#undef D4
#undef D5
#undef D6
#undef C1
#undef C2
#undef C3
#undef C6

std::uint32_t __fastcall d2_grow_fx(void* vec, void*, std::uint32_t end, std::uint32_t src) {
    const std::uint32_t args[] = {end, src};
    logDirect("6f3e20", args, 2);
    if (end >= kBase && end < kBase + 0x4000u && src >= 0x10000u) {
        st(end, ld(src));
    }
    st(reinterpret_cast<std::uint32_t>(vec) + 4, end + 4);
    return popq();
}

std::uint32_t __fastcall d2_push8_fx(void* vec, void*, std::uint32_t end, std::uint32_t src) {
    const std::uint32_t args[] = {end, src};
    logDirect("7f17c0", args, 2);
    if (end >= kBase && end < kBase + 0x4000u && src >= 0x10000u) {
        st(end, ld(src));
        st(end + 4, ld(src + 4));
    }
    st(reinterpret_cast<std::uint32_t>(vec) + 4, end + 8);
    return popq();
}

std::uint32_t __cdecl c1_proj_fx(std::uint32_t ptr) {
    const std::uint32_t args[] = {ptr};
    logDirect("63ed00", args, 1);
    if (g_nudge && ptr >= 0x10000u) {
        st(ptr, ld(ptr) + 1);
    }
    return popq();
}

std::uint32_t __fastcall d2_spos_fx(void* self, void*, std::uint32_t coords, std::uint32_t flag) {
    const std::uint32_t args[] = {coords, flag};
    logDirect("6f0910", args, 2);
    if (coords >= 0x10000u) {
        const auto dst = reinterpret_cast<std::uint32_t>(self) + 0x38;
        st(dst, ld(coords));
        st(dst + 4, ld(coords + 4));
        st(dst + 8, ld(coords + 8));
    }
    return popq();
}

void plant() {
    std::memset(reinterpret_cast<void*>(static_cast<std::uintptr_t>(kBase)), 0, 0x4000);
    st(kVt + 0x08, reinterpret_cast<std::uint32_t>(&v1_08));
    st(kVt + 0x14, reinterpret_cast<std::uint32_t>(&v0_14));
    st(kVt + 0x18, reinterpret_cast<std::uint32_t>(&v1_18));
    st(kVt + 0x2c, reinterpret_cast<std::uint32_t>(&v1_2c));
    st(kVt + 0x58, reinterpret_cast<std::uint32_t>(&v2_58));
    st(kVt + 0x60, reinterpret_cast<std::uint32_t>(&v0_60));
    st(kVt + 0x64, reinterpret_cast<std::uint32_t>(&v1_64));
    st(kVt + 0x6c, reinterpret_cast<std::uint32_t>(&v1_6c1));
    st(kVt + 0x70, reinterpret_cast<std::uint32_t>(&v1_70));
    st(kVt + 0x84, reinterpret_cast<std::uint32_t>(&v1_84));
    st(kVt + 0xac, reinterpret_cast<std::uint32_t>(&v0_ac));
    st(kVt + 0xb4, reinterpret_cast<std::uint32_t>(&v1_b4));
    st(kVt + 0xf4, reinterpret_cast<std::uint32_t>(&v0_f4));
    st(kVt3 + 0x6c, reinterpret_cast<std::uint32_t>(&v0_6c0));
    st(kXfer, kVt);
    st(kSelf, kVt);
    st(kSelf - 0x18, kVt);
    st(kSelf - 0x10, kVt3);
    st(kSelf + 0x16c, kVt4);
    st(kVt4 + 0xf4, reinterpret_cast<std::uint32_t>(&v0_f4));
    st(kVt4 + 0x60, reinterpret_cast<std::uint32_t>(&v0_60));
    st(kXfer, kVt);
    st(kP + 0x14, kP + 0x14);
    st(kP + 0x18, kP + 0x14);
}

ra3::gamelogic::Object70Calls calls() {
    ra3::gamelogic::Object70Calls out{};
    out.freePtr = c1_free;
    out.allocNode = c6_node;
    out.itemDtor = d0_item;
    out.playerHas = d1_phas;
    out.releasePair = d2_rpair;
    out.releaseOne = d0_rone;
    out.playerCount = d2_pcnt;
    out.seedDisguise = d4_seed;
    out.finishPower = d2_fin;
    out.powerReady = d0_pready;
    out.xferBlock = c2_xblock;
    out.projectCoord = c1_proj_fx;
    out.xferNode = d1_xnode;
    out.xferAt10 = d1_x10;
    out.findModule = d1_find;
    out.bridgeCount = d0_bcnt;
    out.liftPrepare = d0_lprep;
    out.upgradeCheck = d2_ucheck;
    out.upgradeApply = d2_uapply;
    out.effectOn = d0_eff;
    out.ammoNotify = d0_ammo;
    out.enterState = d1_enter;
    out.giveTemplate = d2_give;
    out.recallScan = d2_scan;
    out.stancePulse = d0_pulse;
    out.recallTest = d3_recall;
    out.setStatus = d2_status;
    out.stanceFlag = d1_sflag;
    out.proneRefresh = d0_prone;
    out.oclForward = d6_ocl;
    out.pointTest = d3_point;
    out.subRelay = d1_sub;
    out.fireWeapon = d5_fire;
    out.setPosition = d2_spos_fx;
    out.clearSlot = d1_slot;
    out.weaponQuery = d0_wquery;
    out.weaponLink = d2_wlink;
    out.vectorGrow = d2_grow_fx;
    out.liftAttach = d2_latt;
    out.stancePush = d2_spush;
    out.hordeBase = d0_horde;
    out.specialReady = d0_ready;
    out.doorTest = d1_door;
    out.xferTail30 = c2_xt30;
    out.xferTail2c = c2_xt2c;
    out.stopEffect = d0_stop;
    out.oclFinish = d0_ocld;
    out.clearDamage = d0_cdmg;
    out.sphereProbe = d0_sprobe;
    out.sphereAlt = d0_salt;
    out.sphereMain = d0_smain;
    out.assignField = c2_field;
    out.bezierTail = d2_bezier;
    out.push8 = d2_push8_fx;
    out.doorAlt = d1_dalt;
    out.doorScan = d1_dscan;
    out.ghostFind = d1_gfind;
    out.xferPair = c2_xpair;
    out.bridgePlace = d1_bplace;
    out.xferBase = d1_xbase;
    out.moveRange = c3_range;
    out.nameKey = d1_nkey;
    out.moveBytes = c3_bytes;
    return out;
}

ra3::gamelogic::Object70Env envTemplate() {
    ra3::gamelogic::Object70Env env{};
    env.ghostRoot = kP;
    env.namekeyReady = 0;
    env.namekeyValue = 0;
    env.namekeyThis = kVt;
    env.bridgeWorld = kQ;
    env.recallBits = 0x3f800000u;
    env.stanceFloorBits = 0x3f000000u;
    env.prodLimitBits = 0x3f800000u;
    env.player = kR;
    return env;
}

std::uint8_t* self() {
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(kSelf));
}

void publish(const ra3::gamelogic::Object70Env& env) {
    ra3::gamelogic::setObject70(env, calls());
}

std::string memText(const std::vector<std::uint32_t>& offsets) {
    if (offsets.empty()) {
        return ".";
    }
    std::ostringstream text;
    text << std::hex << std::nouppercase;
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        if (i != 0) {
            text << ",";
        }
        text << offsets[i] << ":" << std::setw(8) << std::setfill('0') << ld(kSelf + offsets[i]);
    }
    return text.str();
}

void chainOwner(std::uint32_t rootSlot, std::uint32_t id) {
    st(rootSlot, kP);
    st(kP + 8, kQ);
    st(kQ + 8, kR);
    st(kR, kS);
    st(kS + 4, id);
}

void otherOwner(std::uint32_t id) {
    st(kT + 8, kU);
    st(kU, kS + 0x20);
    st(kS + 0x24, id);
}

void vectorAt(std::uint32_t vec, std::uint32_t begin, std::uint32_t end, std::uint32_t cap) {
    st(vec, begin);
    st(vec + 4, end);
    st(vec + 8, cap);
}

std::string run(const std::string& line) {
    std::istringstream input(line);
    std::string cmd;
    input >> cmd;
    std::vector<std::uint32_t> args;
    std::vector<std::uint32_t> queue;
    std::vector<std::uint32_t> mem;
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
        const auto value = static_cast<std::uint32_t>(std::stoul(token, nullptr, 0));
        if (phase == 1) {
            queue.push_back(value);
        } else if (phase == 2) {
            mem.push_back(value);
        } else {
            args.push_back(value);
        }
    }
    plant();
    g_log.clear();
    g_q = queue;
    g_qi = 0;
    g_forceMinor = -1;
    g_nudge = false;
    g_touch34 = false;
    auto env = envTemplate();
    publish(env);
    auto* me = self();
    std::uint32_t eax = 0;
    if (cmd == "mask") {
        st(kVec, 0);
        eax = ra3::gamelogic::BeamSpecialAbilityUpdate_setMask46(kVec);
        mem = {0x1a00};
    } else if (cmd == "adj4") {
        eax = ra3::gamelogic::BeamSpecialAbilityUpdate_adjustB4(
            reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(args.at(0))));
    } else if (cmd == "adj0") {
        eax = ra3::gamelogic::BeamSpecialAbilityUpdate_adjustB0(
            reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(args.at(0))));
    } else if (cmd == "sameb") {
        chainOwner(kSelf - 0xb8, args.at(0));
        otherOwner(args.at(1));
        eax = ra3::gamelogic::BeamSpecialAbilityUpdate_sameOwner(me, kT);
    } else if (cmd == "samew") {
        chainOwner(kSelf - 0xb4, args.at(0));
        otherOwner(args.at(1));
        eax = ra3::gamelogic::WeaponFireSpecialAbilityUpdate_sameOwner(me, kT);
    } else if (cmd == "span") {
        st(kSelf + 0xb8, args.at(0));
        st(kSelf + 0xbc, args.at(1));
        eax = ra3::gamelogic::OCLMonitorSpecialAbilityUpdate_spanOpen(me);
    } else if (cmd == "fwd") {
        eax = ra3::gamelogic::OCLMonitorSpecialAbilityUpdate_forward(me, args.at(0), args.at(1), args.at(2),
                                                                    args.at(3), args.at(4), args.at(5));
    } else if (cmd == "armed") {
        st(kSelf - 0x20, kP);
        sb(kP + 8, static_cast<std::uint8_t>(args.at(0)));
        eax = ra3::gamelogic::LinearDamageUpdate_relayIfArmed(me, 1);
    } else if (cmd == "unset") {
        st(kP, args.at(1));
        eax = ra3::gamelogic::LinearDamageUpdate_clearIfUnset(me, args.at(0));
    } else if (cmd == "maskif") {
        sb(kSelf + 0x30, static_cast<std::uint8_t>(args.at(0)));
        eax = ra3::gamelogic::SubStructureBuildUpgrade_maskIf(me, 7);
    } else if (cmd == "relay") {
        st(kSelf - 8, kP);
        st(kP + 0xf4, args.at(0));
        eax = ra3::gamelogic::SubObjectsUpgrade_relay(me, 9);
    } else if (cmd == "inside") {
        st(kSelf - 0x1c, kP);
        eax = ra3::gamelogic::SpecialAbilityUpdate_pointInside(me, args.at(0), args.at(1));
    } else if (cmd == "pushid") {
        vectorAt(kSelf + 4, args.at(0), args.at(1), args.at(2));
        eax = ra3::gamelogic::StoreTemplateIDBehavior_pushId(me, args.at(3));
        mem = {0x8, 0x1a00};
    } else if (cmd == "clearrec") {
        st(kSelf + 0xf0, args.at(0));
        st(kSelf + 0xf4, args.at(1));
        eax = ra3::gamelogic::StancesBehavior_clearRecords(me);
        mem = {0xf4};
    } else if (cmd == "stance") {
        env.stanceFloorBits = args.at(0);
        st(kSelf + 4, args.at(1));
        st(kSelf + 0x0c, args.at(2));
        st(kSelf + 0x34, args.at(3));
        st(kSelf - 8, kP);
        st(kP + 0x80, args.at(4));
        g_touch34 = args.size() > 8 && args.at(8) != 0;
        if (args.at(5) != 0) {
            st(kQ + 8, args.at(6));
        }
        publish(env);
        eax = ra3::gamelogic::StancesBehavior_accumulate(me, args.at(7), args.at(5) ? kQ : 0);
        mem = {0x4, 0x8, 0x34};
    } else if (cmd == "latch") {
        st(kSelf + 4, kP);
        sb(kP + 0x101, static_cast<std::uint8_t>(args.at(0)));
        st(kSelf + 8, kQ);
        st(kQ + 0x88, args.at(1));
        eax = ra3::gamelogic::SwitchLocomotorsSpecialAbilityUpdate_latch(me);
        mem = {0x688};
    } else if (cmd == "motion") {
        st(kSelf - 0x20, kP);
        sb(kP + 0x6d, static_cast<std::uint8_t>(args.at(0)));
        st(kSelf - 0x1c, kQ);
        st(kQ + 0x80, args.at(1));
        eax = ra3::gamelogic::BehaviorModule_clearMotion(me, 0);
        mem = {0x4, 0x8, 0x680};
    } else if (cmd == "poll") {
        st(kSelf + 0x20, kP);
        st(kP + 0x3c, args.at(0));
        st(kP + 0x5c, args.at(1));
        sb(kP + 0x60, static_cast<std::uint8_t>(args.at(2)));
        st(kSelf + 0x18, kQ);
        st(kQ + 0x24, kR);
        sb(kR + 0x9b, static_cast<std::uint8_t>(args.at(3)));
        st(kS + 0x94, args.at(4));
        eax = ra3::gamelogic::WeaponReadyState_poll(me);
        mem = {0x43c};
    } else if (cmd == "fire") {
        st(kSelf + 0x18, kQ);
        st(kQ + 0x24, kR);
        st(kR + 0x80, args.at(0));
        st(kSelf + 0x20, kP);
        st(kP + 4, kS);
        st(kS + 4, kT);
        st(kT + 0xb4, args.at(1));
        st(kP + 0x5c, args.at(2));
        st(kP + 0x24, args.at(3));
        sb(kP + 0x60, 1);
        eax = ra3::gamelogic::WeaponFireState_enter(me, 0);
        mem = {0x28, 0x460, 0x880};
    } else if (cmd == "ammo") {
        st(kSelf + 0x20, kP);
        st(kP + 4, kS);
        st(kS + 4, kT);
        st(kT + 0xb4, args.at(0));
        st(kT + 0x70, args.at(1));
        st(kT + 0xb8, args.at(2));
        st(kP + 0x1c, args.at(3));
        st(kSelf + 0x18, kQ);
        st(kQ + 0x24, kR);
        eax = ra3::gamelogic::WeaponOutOfAmmoState_enter(me);
        mem = {0x41c};
    } else if (cmd == "door") {
        st(kSelf - 0x20, kP);
        st(kP + 8, args.at(0));
        st(kSelf + 0x1c, args.at(1));
        st(kSelf - 0x1c, kQ);
        st(kQ + 0x88, args.at(2));
        st(kQ + 0x94, args.at(3));
        st(kQ + 0x418, args.at(4));
        eax = ra3::gamelogic::ProductionUpdate_doorCode(me, 3);
    } else if (cmd == "scan") {
        st(kSelf + 0xfc, args.at(0));
        st(kSelf + 0x100, args.at(1));
        st(kT + 4, 0x11);
        st(kT + 8, 0x22);
        if (args.size() > 2 && args.at(2) != 0) {
            env.prodLimitBits = args.at(3);
            st(kSelf + 0x10, kU);
            sb(kU + 0x65, static_cast<std::uint8_t>(args.at(4)));
            st(kU + 0x1c, args.at(5));
            st(kU + 0x0c, args.at(6));
            st(kU + 0x10, args.at(7));
            st(kU + 0x54, args.at(8) ? kU + 0x80 : 0);
            if (args.at(8)) {
                sb(kU + 0x80 + 0x65, 0);
            }
            publish(env);
            eax = ra3::gamelogic::ProductionUpdate_scanOrPush(me, kT, 1);
        } else {
            publish(env);
            eax = ra3::gamelogic::ProductionUpdate_scanOrPush(me, kT, 0);
            mem = {0xfc, 0x1a00, 0x1a04};
        }
    } else if (cmd == "trap") {
        sb(kSelf + 0x54, static_cast<std::uint8_t>(args.at(0)));
        st(kSelf + 4, kP);
        st(kP + 0x104, args.at(1));
        st(kSelf + 8, kQ);
        st(kQ + 0x38, args.at(2));
        st(kQ + 0x3c, args.at(3));
        st(kQ + 0x40, args.at(4));
        g_nudge = args.size() > 5 && args.at(5) != 0;
        publish(env);
        eax = ra3::gamelogic::BoobyTrapUpdate_capture(me, 0);
        mem = {0x54, 0x638};
    } else if (cmd == "onbit") {
        sb(kSelf + 0x45, static_cast<std::uint8_t>(args.at(0)));
        st(kP, args.at(2));
        st(kSelf - 0x20, kQ);
        eax = ra3::gamelogic::MagneticEffectUpdate_onBit(me, args.at(1));
    } else if (cmd == "refresh") {
        st(kSelf, kVt);
        sb(kSelf + 0x291, static_cast<std::uint8_t>(args.at(0)));
        eax = ra3::gamelogic::HordeContain_refresh(me);
    } else if (cmd == "ids") {
        st(kSelf - 0x168, kP);
        st(kP + 0x128, args.at(0));
        st(kP + 0x12c, kQ);
        st(kQ, args.at(1));
        st(kQ + 4, args.at(2));
        vectorAt(kVec, args.at(3), args.at(4), args.at(5));
        eax = ra3::gamelogic::HordeContain_collectIds(me, reinterpret_cast<std::uint8_t*>(kVec));
        mem = {0x1a04, 0x1a20};
    } else if (cmd == "ghost") {
        env.ghostRoot = kP;
        publish(env);
        eax = ra3::gamelogic::GhostObject_linkWatcher(me);
        mem = {0x18, 0x418, 0x2a00, 0x2a04, 0x2a08};
    } else if (cmd == "seed") {
        eax = ra3::gamelogic::SpecialDisguiseUpdate_seed(me, args.at(0));
    } else if (cmd == "find") {
        st(kSelf - 0x60, kP);
        st(kP + 0xc0, args.at(0));
        env.ghostRoot = kQ;
        publish(env);
        eax = ra3::gamelogic::Object_maskedFind(me);
    } else if (cmd == "filter") {
        st(kSelf - 8, kP);
        st(kP + 0x418, kQ);
        eax = ra3::gamelogic::RecallUnitsSpecialPower_filter(me, args.at(0), args.at(1), 0);
    } else if (cmd == "upgrade") {
        st(kSelf - 0x20, kP);
        st(kP + 0xf8, args.at(0));
        st(kSelf - 0x1c, kQ);
        eax = ra3::gamelogic::GiveOrRestoreUpgradeSpecialPower_try(me, args.at(1), args.at(2));
    } else if (cmd == "bridge") {
        st(kXfer + 4, args.at(0));
        eax = ra3::gamelogic::BridgeBuilderBehavior_xfer(me, reinterpret_cast<std::uint8_t*>(kXfer));
        mem = {0x4c};
    } else if (cmd == "enclose") {
        g_forceMinor = static_cast<int>(args.at(0));
        publish(env);
        eax = ra3::gamelogic::EncloseBehavior_xfer(me, reinterpret_cast<std::uint8_t*>(kXfer));
    } else if (cmd == "shield") {
        st(kXfer + 4, args.at(0));
        g_forceMinor = static_cast<int>(args.at(1));
        publish(env);
        eax = ra3::gamelogic::ShieldSphereUpdate_xfer(me, reinterpret_cast<std::uint8_t*>(kXfer));
    } else if (cmd == "bombard") {
        st(kXfer + 4, args.at(0));
        st(kSelf + 0x2c, args.at(1));
        st(kSelf + 0x30, args.at(2));
        eax = ra3::gamelogic::BombardAreaUpdate_xfer(me, reinterpret_cast<std::uint8_t*>(kXfer));
    } else if (cmd == "mxfer") {
        g_forceMinor = static_cast<int>(args.at(0));
        publish(env);
        eax = ra3::gamelogic::MagneticEffectUpdate_xfer(me, reinterpret_cast<std::uint8_t*>(kXfer));
    } else if (cmd == "beamx") {
        eax = ra3::gamelogic::BeamTargetModule_xfer(me, reinterpret_cast<std::uint8_t*>(kXfer));
    } else if (cmd == "lift") {
        st(kXfer + 4, args.at(0));
        st(kSelf + 0x2c, args.at(1) ? kU : kSelf + 0x2c);
        if (args.at(1)) {
            st(kU, args.at(2) ? kT : kSelf + 0x2c);
            if (args.at(2)) {
                st(kT, kSelf + 0x2c);
            }
        }
        eax = ra3::gamelogic::LiftObjectUpdate_xfer(me, reinterpret_cast<std::uint8_t*>(kXfer));
    } else if (cmd == "emotion") {
        st(kSelf + 0x78, args.at(0));
        st(kSelf + 0x68, kVec);
        st(kSelf + 0x6c, kVec + args.at(1));
        st(kVec, args.at(2));
        st(kVec + 4, args.at(3));
        eax = ra3::gamelogic::EmotionTrackerUpdate_release(me);
        mem = {0x6c, 0x78};
    } else if (cmd == "clearid") {
        st(kSelf + 8, args.at(0));
        st(kSelf + 0x0c, args.at(1));
        eax = ra3::gamelogic::NotifyWhenAttackingBehavior_clearIds(me, 0);
        mem = {0xc};
    } else if (cmd == "power") {
        st(kSelf + 4, kP);
        st(kP + 0x38, args.at(0));
        st(kP + 0x10, args.at(1));
        st(kP + 0x18, args.at(2));
        st(kQ + 0x3cc, args.at(3));
        st(kQ + 4, kR);
        st(kR + 0xcc, args.at(4));
        env.player = kS;
        publish(env);
        eax = ra3::gamelogic::SpecialPowerModule_trigger(me, reinterpret_cast<std::uint8_t*>(kQ));
    } else if (cmd == "prone") {
        env.namekeyReady = args.at(0);
        env.namekeyValue = args.at(1);
        st(kSelf + 8, kP);
        st(kT + 0x8c, args.at(2));
        st(kU + 0x24, args.at(3));
        st(kU + 4, kS);
        st(kS + 8, args.at(4));
        publish(env);
        eax = ra3::gamelogic::AIUpdateInterface_applyProne(me, reinterpret_cast<std::uint8_t*>(kT));
        ra3::gamelogic::Object70Env after{};
        ra3::gamelogic::object70Env(after);
        st(kSelf + 0x3f0, after.namekeyReady);
        st(kSelf + 0x3f4, after.namekeyValue);
        mem = {0x3f0, 0x3f4, 0xe24};
    } else if (cmd == "aim") {
        st(kSelf - 0x1c, args.at(0) ? kP : 0);
        st(kQ, args.at(1));
        st(kQ + 4, args.at(2));
        st(kQ + 8, args.at(3));
        st(kP + 0x38, 0x10);
        st(kP + 0x3c, 0x20);
        st(kP + 0x40, 0x30);
        eax = ra3::gamelogic::BezierProjectileBehavior_copyAim(me, kQ, args.at(4));
        mem = {0x34, 0x40, 0x60};
    } else if (cmd == "sphere") {
        eax = ra3::gamelogic::ShieldSphereManager_retarget(me);
    } else {
        return "eax=00000000 log=. mem=.";
    }
    std::ostringstream out;
    out << std::hex << std::nouppercase;
    out << "eax=" << std::setw(8) << std::setfill('0') << eax;
    out << " log=" << (g_log.empty() ? "." : g_log);
    out << " mem=" << memText(mem);
    return out.str();
}

}  // namespace

int main() {
    if (VirtualAlloc(reinterpret_cast<void*>(static_cast<std::uintptr_t>(kBase)), 0x4000,
                      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == nullptr) {
        std::cerr << "arena alloc failed\n";
        return 1;
    }
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit" || line.empty()) {
            break;
        }
        std::cout << run(line) << "\n" << std::flush;
    }
    return 0;
}
