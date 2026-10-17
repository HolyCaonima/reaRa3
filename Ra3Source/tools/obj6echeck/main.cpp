#include "ra3/gamelogic/ObjectModules6E.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string g_log;
int g_fault = 0;
std::vector<std::string> g_rets;
std::map<std::uint32_t, int> g_table;
int g_mix = 0;
std::string g_mode = "full";
bool g_patchOn = false;
std::uint32_t g_patch = 0;
std::uint32_t g_word = 0;
std::uint32_t g_cell = 0;

struct Obj {
    std::uint8_t* base = nullptr;
    std::uint8_t* data = nullptr;
    std::size_t size = 0;

    Obj() = default;
    Obj(const Obj&) = delete;
    Obj& operator=(const Obj&) = delete;
    Obj(Obj&& other) noexcept : base(other.base), data(other.data), size(other.size) {
        other.base = nullptr;
        other.data = nullptr;
    }
    Obj& operator=(Obj&& other) noexcept {
        if (this != &other) {
            release();
            base = other.base;
            data = other.data;
            size = other.size;
            other.base = nullptr;
            other.data = nullptr;
        }
        return *this;
    }
    ~Obj() { release(); }
    void release() noexcept {
        if (base != nullptr) {
            VirtualFree(base, 0, MEM_RELEASE);
            base = nullptr;
            data = nullptr;
        }
    }
};

Obj allocateObj(std::size_t bytes) {
    Obj made;
    const std::size_t page = 4096;
    const std::size_t commit = (bytes + page - 1) & ~(page - 1);
    made.base = static_cast<std::uint8_t*>(
        VirtualAlloc(nullptr, commit + page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD previous = 0;
    VirtualProtect(made.base + commit, page, PAGE_NOACCESS, &previous);
    made.size = bytes;
    made.data = made.base + (commit - bytes);
    std::memset(made.data, 0x11, bytes);
    return made;
}

std::map<int, Obj> g_objs;
std::vector<std::vector<std::uint8_t>> g_tables;
int g_watch = 0;
std::vector<int> g_offs;

void addLog(const std::string& text) {
    if (!g_log.empty()) {
        g_log += ",";
    }
    g_log += text;
}

std::string hx(std::uint32_t value) {
    std::ostringstream text;
    text << std::hex << std::nouppercase << value;
    return text.str();
}

std::string pv(std::uint32_t value) { return value == 0 ? "0" : "p"; }

std::uint32_t loadU32(const void* address) {
    std::uint32_t value = 0;
    std::memcpy(&value, address, 4);
    return value;
}

void storeU32(void* address, std::uint32_t value) { std::memcpy(address, &value, 4); }

std::uint8_t* asPtr(std::uint32_t value) {
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(value));
}

std::uint32_t asBits(const void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint8_t* objPtr(int id) { return g_objs.at(id).data; }

std::uint32_t popRaw() {
    if (g_rets.empty()) {
        return 0;
    }
    const std::string token = g_rets.front();
    g_rets.erase(g_rets.begin());
    if (!token.empty() && token[0] == '@') {
        return asBits(objPtr(std::atoi(token.c_str() + 1)));
    }
    return static_cast<std::uint32_t>(std::strtoul(token.c_str(), nullptr, 0));
}

std::uint32_t onN(int slot) {
    addLog("v" + hx(static_cast<std::uint32_t>(slot)));
    return popRaw();
}

std::uint32_t onU(int slot, std::uint32_t arg) {
    addLog("v" + hx(static_cast<std::uint32_t>(slot)) + ":" + hx(arg));
    return popRaw();
}

std::uint32_t onP(int slot, std::uint32_t arg) {
    addLog("v" + hx(static_cast<std::uint32_t>(slot)) + ":" + pv(arg));
    return popRaw();
}

std::uint32_t onW(int slot, std::uint32_t first, std::uint32_t second) {
    addLog("v" + hx(static_cast<std::uint32_t>(slot)) + ":" + hx(first) + ":" + hx(second));
    return popRaw();
}

std::uint32_t onM(int slot, std::uint32_t first, std::uint32_t second) {
    addLog("v" + hx(static_cast<std::uint32_t>(slot)) + ":" + pv(first) + ":" + hx(second));
    return popRaw();
}

std::uint32_t onS(int slot, std::uint32_t field) {
    addLog("v" + hx(static_cast<std::uint32_t>(slot)) + ":p");
    const std::uint32_t value = popRaw();
    storeU32(asPtr(field), value);
    return value;
}

std::uint32_t onQ(int slot) {
    addLog("v" + hx(static_cast<std::uint32_t>(slot)) + ":p:0");
    g_word = popRaw();
    return asBits(&g_word);
}

#define DECL_N(name, slot) \
    std::uint32_t __fastcall name(void*, void*) { return onN(slot); }
#define DECL_U(name, slot) \
    std::uint32_t __fastcall name(void*, void*, std::uint32_t arg) { return onU(slot, arg); }
#define DECL_P(name, slot) \
    std::uint32_t __fastcall name(void*, void*, std::uint32_t arg) { return onP(slot, arg); }
#define DECL_W(name, slot) \
    std::uint32_t __fastcall name(void*, void*, std::uint32_t a, std::uint32_t b) { return onW(slot, a, b); }
#define DECL_M(name, slot) \
    std::uint32_t __fastcall name(void*, void*, std::uint32_t a, std::uint32_t b) { return onM(slot, a, b); }
#define DECL_S(name, slot) \
    std::uint32_t __fastcall name(void*, void*, std::uint32_t field) { return onS(slot, field); }
#define DECL_Q(name, slot) \
    std::uint32_t __fastcall name(void*, void*, std::uint32_t, std::uint32_t) { return onQ(slot); }

DECL_N(vn_10, 0x10) DECL_N(vn_14, 0x14) DECL_N(vn_18, 0x18) DECL_N(vn_1c, 0x1C)
DECL_N(vn_20, 0x20) DECL_N(vn_24, 0x24) DECL_N(vn_28, 0x28) DECL_N(vn_30, 0x30)
DECL_N(vn_34, 0x34) DECL_N(vn_4c, 0x4C) DECL_N(vn_64, 0x64) DECL_N(vn_6c, 0x6C)
DECL_N(vn_70, 0x70) DECL_N(vn_84, 0x84) DECL_N(vn_b0, 0xB0) DECL_N(vn_b4, 0xB4)
DECL_N(vn_20c, 0x20C)
DECL_U(vu_1c, 0x1C) DECL_U(vu_28, 0x28) DECL_U(vu_3c, 0x3C) DECL_U(vu_5c, 0x5C)
DECL_U(vu_b0, 0xB0) DECL_U(vu_10c, 0x10C)
DECL_P(vp_18, 0x18) DECL_P(vp_1c, 0x1C) DECL_P(vp_38, 0x38)
DECL_W(vw_18, 0x18)
DECL_M(vm_08, 0x08) DECL_M(vm_58, 0x58)
DECL_S(vs_6c, 0x6C) DECL_S(vs_70, 0x70)
DECL_Q(vq_ac, 0xAC)

void __fastcall onLock(void*, void*) { addLog("lk"); }
void __fastcall onUnlock(void*, void*) { addLog("ul"); }

std::uint32_t __fastcall onFind(void*, void*, std::uint32_t key) {
    addLog("fd:" + hx(key));
    const auto found = g_table.find(key);
    if (found == g_table.end() || found->second == 0) {
        return 0;
    }
    return asBits(objPtr(found->second));
}

std::uint32_t __fastcall on4d3b50(void*, void*, std::uint32_t arg) {
    addLog("d4d3b50:" + hx(arg));
    return popRaw();
}
void __fastcall on6a4fb0(void*, void*) { addLog("d6a4fb0"); }
std::uint32_t __fastcall on687720(void*, void*, std::uint32_t arg) {
    addLog("d687720:" + hx(arg));
    return popRaw();
}
std::uint32_t __fastcall on6d1ed0(void*, void*, std::uint32_t a, std::uint32_t b) {
    addLog("d6d1ed0:" + hx(a) + ":" + hx(b));
    return popRaw();
}
std::uint32_t __fastcall on687560(void*, void*) {
    addLog("d687560");
    return popRaw();
}
void __fastcall on54f490(void*, void*, std::uint32_t arg) { addLog("d54f490:" + hx(arg)); }
std::uint32_t __fastcall on6c0ed0(void*, void*) {
    addLog("d6c0ed0");
    return popRaw();
}
std::uint32_t __fastcall on5ac4f0(void*, void*, std::uint32_t a, std::uint32_t b) {
    addLog("d5ac4f0:" + hx(a) + ":" + hx(b));
    return 0;
}
std::uint32_t __fastcall on6cc2e0(void*, void*, std::uint32_t arg) {
    addLog("d6cc2e0:" + pv(arg));
    return popRaw();
}
std::uint32_t __fastcall on6cace0(void*, void*, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    addLog("d6cace0:" + hx(a) + ":" + hx(b) + ":" + hx(c));
    return popRaw();
}
std::uint32_t __fastcall on6a7b60(void*, void*, std::uint32_t a, std::uint32_t b) {
    addLog("d6a7b60:" + hx(a) + ":" + hx(b));
    return popRaw();
}
std::uint32_t __fastcall on6cab20(void*, void*) {
    addLog("d6cab20");
    return popRaw();
}
std::uint32_t __fastcall on6ead80(void*, void*) {
    addLog("d6ead80");
    return popRaw();
}
void __fastcall on6e6830(void*, void*, std::uint32_t arg) { addLog("d6e6830:" + pv(arg)); }
void __fastcall on8d82f0(void*, void*, std::uint32_t) { addLog("d8d82f0:p"); }
void __fastcall on685080(void*, void*, std::uint32_t) { addLog("d685080:p"); }
std::uint32_t __fastcall on6e2470(void*, void*, std::uint32_t) {
    addLog("d6e2470:p");
    return popRaw();
}
std::uint32_t __fastcall on6d3a00(void*, void*, std::uint32_t) {
    addLog("d6d3a00:p");
    return popRaw();
}
std::uint32_t __fastcall on6d3870(void*, void*, std::uint32_t) {
    addLog("d6d3870:p");
    return popRaw();
}
void __fastcall on6a8400(void*, void*) { addLog("d6a8400"); }
void __fastcall on6bcf30(void*, void*, std::uint32_t arg) { addLog("d6bcf30:" + hx(arg)); }
void __fastcall on6d0330(void*, void*) { addLog("d6d0330"); }
std::uint32_t __fastcall on6a88c0(void*, void*, std::uint32_t arg) {
    addLog("d6a88c0:" + hx(arg));
    return popRaw();
}
std::uint32_t __fastcall on6d6ec0(void*, void*) {
    addLog("d6d6ec0");
    return popRaw();
}
std::uint32_t __fastcall on6d9560(void* self, void*, std::uint32_t arg) {
    addLog("d6d9560:" + pv(arg));
    if (g_patchOn) {
        storeU32(static_cast<std::uint8_t*>(self) + 0x14, g_patch);
    }
    return popRaw();
}
std::uint32_t __fastcall on6efe40(void*, void*) {
    addLog("d6efe40");
    return popRaw();
}
std::uint32_t __fastcall onAafe10(void* self, void*) {
    addLog("daafe10");
    const std::uint32_t flag = popRaw();
    g_cell = flag == 1 ? asBits(static_cast<std::uint8_t*>(self) + 4) : 0;
    return asBits(&g_cell);
}
std::uint32_t __fastcall on6f2140(void*, void*, std::uint32_t a, std::uint32_t b, std::uint32_t c,
                                  std::uint32_t d) {
    addLog("d6f2140:" + hx(a) + ":" + hx(b) + ":" + hx(c) + ":" + hx(d));
    return popRaw();
}
std::uint32_t __fastcall on6fa190(void*, void*, std::uint32_t arg) {
    addLog("d6fa190:" + hx(arg));
    return popRaw();
}
std::uint32_t __fastcall on6e0450(void*, void*, std::uint32_t arg) {
    addLog("d6e0450:" + hx(arg));
    return popRaw();
}
std::uint32_t __fastcall on6e1630(void*, void*) {
    addLog("d6e1630");
    return 0;
}
std::uint32_t __fastcall on6eaef0(void*, void*) {
    addLog("d6eaef0");
    return popRaw();
}
void __fastcall on6ff750(void*, void*) { addLog("d6ff750"); }
void __fastcall on52dde0(void*, void*, std::uint32_t a, std::uint32_t b) {
    addLog("d52dde0:" + hx(a) + ":" + hx(b));
}
std::uint32_t __cdecl on6e3a60(std::uint32_t a, std::uint32_t b) {
    addLog("d6e3a60:" + hx(a) + ":" + hx(b));
    return popRaw();
}
std::uint32_t __fastcall on6c0e50(void*, void*, std::uint32_t arg) {
    addLog("d6c0e50:" + hx(arg));
    return popRaw();
}
std::uint32_t __fastcall on6e90b0(void*, void*, std::uint32_t a, std::uint32_t b) {
    addLog("d6e90b0:" + hx(a) + ":" + hx(b));
    return popRaw();
}
std::uint32_t __fastcall on69d570(void*, void*, std::uint32_t arg) {
    addLog("d69d570:" + pv(arg));
    return popRaw();
}
std::uint32_t __fastcall on6f1350(void*, void*) {
    addLog("d6f1350");
    return popRaw();
}

using Fn = void*;
std::map<std::string, Fn> g_fns;

void buildFns() {
    g_fns["10:n"] = (Fn)vn_10;
    g_fns["14:n"] = (Fn)vn_14;
    g_fns["18:n"] = (Fn)vn_18;
    g_fns["1c:n"] = (Fn)vn_1c;
    g_fns["20:n"] = (Fn)vn_20;
    g_fns["24:n"] = (Fn)vn_24;
    g_fns["28:n"] = (Fn)vn_28;
    g_fns["30:n"] = (Fn)vn_30;
    g_fns["34:n"] = (Fn)vn_34;
    g_fns["4c:n"] = (Fn)vn_4c;
    g_fns["64:n"] = (Fn)vn_64;
    g_fns["6c:n"] = (Fn)vn_6c;
    g_fns["70:n"] = (Fn)vn_70;
    g_fns["84:n"] = (Fn)vn_84;
    g_fns["b0:n"] = (Fn)vn_b0;
    g_fns["b4:n"] = (Fn)vn_b4;
    g_fns["20c:n"] = (Fn)vn_20c;
    g_fns["1c:u"] = (Fn)vu_1c;
    g_fns["28:u"] = (Fn)vu_28;
    g_fns["3c:u"] = (Fn)vu_3c;
    g_fns["5c:u"] = (Fn)vu_5c;
    g_fns["b0:u"] = (Fn)vu_b0;
    g_fns["10c:u"] = (Fn)vu_10c;
    g_fns["18:p"] = (Fn)vp_18;
    g_fns["1c:p"] = (Fn)vp_1c;
    g_fns["38:p"] = (Fn)vp_38;
    g_fns["18:w"] = (Fn)vw_18;
    g_fns["8:m"] = (Fn)vm_08;
    g_fns["58:m"] = (Fn)vm_58;
    g_fns["6c:s"] = (Fn)vs_6c;
    g_fns["70:s"] = (Fn)vs_70;
    g_fns["ac:q"] = (Fn)vq_ac;
}

std::uint32_t parseU32(const std::string& token) {
    if (!token.empty() && token[0] == '@') {
        return asBits(objPtr(std::atoi(token.c_str() + 1)));
    }
    return static_cast<std::uint32_t>(std::strtoul(token.c_str(), nullptr, 0));
}

std::string take(std::istringstream& input) {
    std::string token;
    input >> token;
    return token;
}

std::string hex8(std::uint32_t value) {
    std::ostringstream text;
    text << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << value;
    return text.str();
}

std::string memList() {
    if (g_watch == 0 || g_offs.empty()) {
        return ".";
    }
    auto* base = objPtr(g_watch);
    std::ostringstream text;
    text << std::hex << std::nouppercase;
    for (std::size_t i = 0; i < g_offs.size(); ++i) {
        if (i != 0) {
            text << ",";
        }
        text << g_offs[i] << ":" << std::setw(8) << std::setfill('0') << loadU32(base + g_offs[i]);
    }
    return text.str();
}

std::string mixOf(std::uint32_t eax) {
    if (eax == 0) {
        return "0";
    }
    if (g_mix == 0 || g_objs.count(g_mix) == 0) {
        return "miss";
    }
    const auto base = asBits(objPtr(g_mix));
    const auto size = static_cast<std::uint32_t>(g_objs[g_mix].size);
    if (eax >= base && eax - base < size) {
        return hx(eax - base);
    }
    return "miss";
}

void resetWorld() {
    g_log.clear();
    g_rets.clear();
    g_table.clear();
    g_objs.clear();
    g_tables.clear();
    g_offs.clear();
    g_watch = 0;
    g_mix = 0;
    g_mode = "full";
    g_patchOn = false;
    g_patch = 0;
    ra3::gamelogic::setObject6EClock(nullptr);
    ra3::gamelogic::setObject6ESt0(0);
}

std::uint32_t invokeBody(std::uint32_t va, std::uint8_t* self, std::uint32_t a, std::uint32_t b,
                         std::uint32_t c) {
    using namespace ra3::gamelogic;
    switch (va) {
    case 0x006E1C80: return MovementSpecialPower_adjustedField(self, nullptr);
    case 0x006E1D30: return MoveToPositionAndEvacuateSpecialPower_dispatch(self, nullptr, a, b, c);
    case 0x006E1E40: return MoveToPositionAndReplaceSelfSpecialPower_dispatch(self, nullptr, a, b, c);
    case 0x006E1F70: return OCLSpecialPower_dispatch(self, nullptr, a, b, c);
    case 0x006E2F30: return SpecialPowerModule_dispatchIfBit25(self, nullptr);
    case 0x006E2F70: return SpecialPowerModule_dispatchUnlessBit6(self, nullptr);
    case 0x006E3220: return ToggleHiddenSpecialAbilityUpdate_clearHidden(self, nullptr);
    case 0x006E3290: return ToggleHiddenSpecialAbilityUpdate_applyHidden(self, nullptr);
    case 0x006E3E10: return DozerAIUpdate_acceptsObject(self, nullptr, a);
    case 0x006E4220: return DozerAIUpdate_resetAction(self, nullptr, a);
    case 0x006E44D0: return DozerAIUpdate_workerFromId(self, nullptr);
    case 0x006E46E0: return HordeAIUpdate_flagPassenger(self, nullptr, a, b);
    case 0x006E48F0: return SupplyTruckAIUpdate_acceptTarget(self, nullptr, a);
    case 0x006E4920: return SupplyTruckAIUpdate_deliverBoxes(self, nullptr, a, b);
    case 0x006E4980: return SupplyTruckAIUpdate_deliverPosition(self, nullptr, a, b);
    case 0x006E4A10: return SupplyTruckAIUpdate_recordCargo(self, nullptr, a, b);
    case 0x006E8750: return MoneyGainAttachUpdate_maybeGain(self, nullptr, a);
    case 0x006E8FD0: return ProductionUpdate_producerField(self, nullptr, a);
    case 0x006E9010: return ProductionUpdate_incrementQueued(self, nullptr);
    case 0x006E9040: return ProductionUpdate_objectFromId(self, nullptr);
    case 0x006E91F0: return ProductionUpdate_clearLink(self, nullptr, a);
    case 0x006E9390: return ProductionUpdate_bindProducer(self, nullptr);
    case 0x006E9EE0: return BehaviorModule_objectOrigin(self, nullptr);
    case 0x006EB4E0: return SpecialAbilityUpdate_isReady(self, nullptr);
    case 0x006EBAC0: return StealUnitUpdate_syncTarget(self, nullptr, a);
    case 0x006EBBD0: return StoreCurrentStateUpdate_xfer(self, nullptr, a);
    case 0x006ECE70: return EncloseBehavior_offerToContainer(self, nullptr, a);
    case 0x006ED440: return WeaponInitState_enter(self, nullptr);
    case 0x006ED4A0: return WeaponPreFireState_clearFlag(self, nullptr, a);
    case 0x006ED4E0: return WeaponFireState_advance(self, nullptr);
    case 0x006F6C10: return GateProxyBehavior_call14(self, nullptr);
    case 0x006F6C30: return GateProxyBehavior_call18(self, nullptr);
    case 0x006F6C50: return GateProxyBehavior_call1C(self, nullptr);
    case 0x006F6C70: return GateProxyBehavior_call10(self, nullptr);
    case 0x006F6C90: return GateProxyBehavior_call20(self, nullptr);
    case 0x006F7DB0: return SpawnBehavior_anyLiving(self, nullptr);
    case 0x006F8A60: return UnifiedControlBehavior_onRadius(self, nullptr, a);
    case 0x006F94B0: return HordeContain_passengerSlotB0(self, nullptr);
    case 0x006F94F0: return HordeContain_passengerSlot28(self, nullptr);
    case 0x006F9710: return HordeContain_knowsObject(self, nullptr, a);
    case 0x006F9B00: return HordeContain_testTriple(self, nullptr, a, b, c);
    case 0x006F9B40: return HordeContain_passengerSlot70(self, nullptr, a, b, c);
    case 0x006FA9E0: return HealContain_noteFrame(self, nullptr, a);
    case 0x006FBB50: return OpenContain_passengerFlag(self, nullptr);
    case 0x006FCAD0: return ActivateModuleSpecialAbilityUpdate_enable(self, nullptr);
    case 0x006FDA30: return GiveUpgradeUpdate_runGrant(self, nullptr);
    case 0x006FE070: return ToggleMountedSpecialAbilityUpdate_mountedReady(self, nullptr);
    case 0x006FF060: return SupplyTruckAIUpdate_popPending(self, nullptr);
    case 0x006FF9F0: return AIUpdateInterface_allowsMove(self, nullptr, a);
    case 0x006FFAD0: return AIUpdateInterface_locomotorReady(self, nullptr);
    case 0x006FFD50: return AIUpdateInterface_orderState33(self, nullptr, a, b);
    default:
        std::cerr << "unknown " << std::hex << va << "\n";
        return 0;
    }
}

std::uint32_t invoke(std::uint32_t va, std::uint8_t* self, std::uint32_t a, std::uint32_t b,
                     std::uint32_t c) {
    g_fault = 0;
    __try {
        return invokeBody(va, self, a, b, c);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_fault = 1;
        return 0;
    }
}

}  // namespace

int main() {
    using namespace ra3::gamelogic;
    std::cout << std::unitbuf;
    buildFns();
    Object6ECalls calls{};
    calls.lock = onLock;
    calls.unlock = onUnlock;
    calls.find = onFind;
    calls.d4d3b50 = on4d3b50;
    calls.d6a4fb0 = on6a4fb0;
    calls.d687720 = on687720;
    calls.d6d1ed0 = on6d1ed0;
    calls.d687560 = on687560;
    calls.d54f490 = on54f490;
    calls.d6c0ed0 = on6c0ed0;
    calls.d5ac4f0 = on5ac4f0;
    calls.d6cc2e0 = on6cc2e0;
    calls.d6cace0 = on6cace0;
    calls.d6a7b60 = on6a7b60;
    calls.d6cab20 = on6cab20;
    calls.d6ead80 = on6ead80;
    calls.d6e6830 = on6e6830;
    calls.d8d82f0 = on8d82f0;
    calls.d685080 = on685080;
    calls.d6e2470 = on6e2470;
    calls.d6d3a00 = on6d3a00;
    calls.d6d3870 = on6d3870;
    calls.d6a8400 = on6a8400;
    calls.d6bcf30 = on6bcf30;
    calls.d6d0330 = on6d0330;
    calls.d6a88c0 = on6a88c0;
    calls.d6d6ec0 = on6d6ec0;
    calls.d6d9560 = on6d9560;
    calls.d6efe40 = on6efe40;
    calls.daafe10 = onAafe10;
    calls.d6f2140 = on6f2140;
    calls.d6fa190 = on6fa190;
    calls.d6e0450 = on6e0450;
    calls.d6e1630 = on6e1630;
    calls.d6eaef0 = on6eaef0;
    calls.d6ff750 = on6ff750;
    calls.d52dde0 = on52dde0;
    calls.d6e3a60 = on6e3a60;
    calls.d6c0e50 = on6c0e50;
    calls.d6e90b0 = on6e90b0;
    calls.d69d570 = on69d570;
    calls.d6f1350 = on6f1350;
    setObject6ECalls(calls);

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
        if (cmd == "reset") {
            resetWorld();
            continue;
        }
        if (cmd == "obj") {
            const int id = std::atoi(take(input).c_str());
            const std::size_t size = static_cast<std::size_t>(parseU32(take(input)));
            g_objs[id] = allocateObj(size);
            continue;
        }
        if (cmd == "u32") {
            const int id = std::atoi(take(input).c_str());
            const int off = static_cast<int>(parseU32(take(input)));
            const std::uint32_t value = parseU32(take(input));
            storeU32(objPtr(id) + off, value);
            continue;
        }
        if (cmd == "u8") {
            const int id = std::atoi(take(input).c_str());
            const int off = static_cast<int>(parseU32(take(input)));
            objPtr(id)[off] = static_cast<std::uint8_t>(parseU32(take(input)));
            continue;
        }
        if (cmd == "ptr") {
            const int id = std::atoi(take(input).c_str());
            const int off = static_cast<int>(parseU32(take(input)));
            const int other = std::atoi(take(input).c_str());
            storeU32(objPtr(id) + off, other == 0 ? 0 : asBits(objPtr(other)));
            continue;
        }
        if (cmd == "ext") {
            const int id = std::atoi(take(input).c_str());
            const int off = static_cast<int>(parseU32(take(input)));
            const int other = std::atoi(take(input).c_str());
            const int off2 = static_cast<int>(parseU32(take(input)));
            storeU32(objPtr(id) + off, asBits(objPtr(other) + off2));
            continue;
        }
        if (cmd == "vt") {
            const int id = std::atoi(take(input).c_str());
            const int base = static_cast<int>(parseU32(take(input)));
            const std::string spec = take(input);
            g_tables.emplace_back(0x400, static_cast<std::uint8_t>(0));
            auto* table = g_tables.back().data();
            std::stringstream parts(spec);
            std::string item;
            while (std::getline(parts, item, ',')) {
                const auto colon = item.find(':');
                const int slot = static_cast<int>(std::strtoul(item.substr(0, colon).c_str(), nullptr, 16));
                const std::string key = hx(static_cast<std::uint32_t>(slot)) + item.substr(colon);
                storeU32(table + slot, asBits(g_fns.at(key)));
            }
            storeU32(objPtr(id) + base, asBits(table));
            continue;
        }
        if (cmd == "gclock") {
            setObject6EClock(objPtr(std::atoi(take(input).c_str())));
            continue;
        }
        if (cmd == "map") {
            const std::uint32_t key = parseU32(take(input));
            const int id = std::atoi(take(input).c_str());
            g_table[key] = id;
            continue;
        }
        if (cmd == "ret") {
            g_rets.push_back(take(input));
            continue;
        }
        if (cmd == "st0") {
            setObject6ESt0(parseU32(take(input)));
            continue;
        }
        if (cmd == "patch") {
            g_patch = parseU32(take(input));
            g_patchOn = true;
            continue;
        }
        if (cmd == "watch") {
            g_watch = std::atoi(take(input).c_str());
            const std::string spec = take(input);
            g_offs.clear();
            std::stringstream parts(spec);
            std::string item;
            while (std::getline(parts, item, ',')) {
                g_offs.push_back(static_cast<int>(std::strtoul(item.c_str(), nullptr, 0)));
            }
            continue;
        }
        if (cmd == "mode") {
            g_mode = take(input);
            continue;
        }
        if (cmd == "mix") {
            g_mix = std::atoi(take(input).c_str());
            continue;
        }
        if (cmd == "run") {
            try {
            const std::uint32_t va = parseU32(take(input));
            const std::string ecxSpec = take(input);
            std::uint8_t* self = nullptr;
            if (!ecxSpec.empty() && ecxSpec[0] == 'o') {
                const auto plus = ecxSpec.find('+');
                const int id = std::atoi(ecxSpec.c_str() + 1);
                const int off = plus == std::string::npos
                                    ? 0
                                    : static_cast<int>(std::strtoul(ecxSpec.c_str() + plus + 1, nullptr, 0));
                self = objPtr(id) + off;
            } else if (!ecxSpec.empty() && ecxSpec[0] == 'r') {
                self = asPtr(static_cast<std::uint32_t>(std::strtoul(ecxSpec.c_str() + 1, nullptr, 0)));
            }
            std::uint32_t args[3] = {};
            std::string token;
            int count = 0;
            while (input >> token && count < 3) {
                args[count++] = parseU32(token);
            }
            const std::uint32_t eax = invoke(va, self, args[0], args[1], args[2]);
            if (g_fault != 0) {
                g_log = "fault";
            }
            std::cout << "eax=" << hex8(eax) << " al=" << std::hex << std::nouppercase << std::setw(2)
                      << std::setfill('0') << (eax & 0xFF) << " log=" << (g_log.empty() ? "." : g_log)
                      << " mem=" << memList();
            if (g_mode == "mix") {
                std::cout << " mix=" << mixOf(eax);
            }
            std::cout << "\n";
            } catch (const std::exception& error) {
                std::cout << "eax=00000000 al=00 log=err mem=. err=" << error.what() << "\n";
            }
        }
    }
    return 0;
}
