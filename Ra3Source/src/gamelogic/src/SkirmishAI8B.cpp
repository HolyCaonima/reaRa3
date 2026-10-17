#include "ra3/gamelogic/SkirmishAI8B.h"

#include <cstdint>
#include <cstring>

namespace ra3::gamelogic {
namespace {

std::uint32_t load32(const std::uint8_t* base, std::uint32_t offset) noexcept {
    std::uint32_t value = 0;
    std::memcpy(&value, base + offset, 4);
    return value;
}

void store32(std::uint8_t* base, std::uint32_t offset, std::uint32_t value) noexcept {
    std::memcpy(base + offset, &value, 4);
}

std::uint8_t* asPtr(std::uint32_t value) noexcept {
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(value));
}

std::uint32_t asU(const void* pointer) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t tacticTable(std::uint32_t ai) noexcept {
    const auto* inner = asPtr(load32(asPtr(ai), 0x10));
    return load32(inner, 0x3C);
}

std::uint32_t tacticWord(std::uint32_t ai, std::uint32_t player, std::uint32_t add) noexcept {
    const auto* table = asPtr(tacticTable(ai));
    const std::uint32_t index = player * 17u;
    return load32(table, index * 4u + add);
}

std::uint8_t* playerFrom(const std::uint8_t* self) noexcept {
    auto* owner = asPtr(load32(self, 0x2C));
    return asPtr(load32(owner, 0x90));
}

void versionPair(SkirmishCalls& calls, std::uint8_t* pair, std::uint8_t major,
                 std::uint8_t minor) noexcept {
    pair[0] = major;
    pair[1] = minor;
    calls.xfer.version(calls.xfer.xfer, pair);
}

bool loadFlag(const SkirmishCalls& calls) noexcept {
    const auto flags = load32(static_cast<const std::uint8_t*>(calls.xfer.xfer), 4);
    return ((flags >> 1) & 1u) != 0u;
}

std::uint32_t createSized(std::uint8_t* self, std::uint32_t bytes, std::uint32_t ctor,
                          SkirmishCalls& calls) noexcept {
    auto* obj = calls.allocate(bytes);
    if (obj == nullptr) {
        return 0;
    }
    const auto player = load32(self, 0x54);
    const auto ai = load32(self, 0x50);
    return calls.construct3(ctor, obj, ai, player, asU(self + 0x38));
}

void bindBuildTarget(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    self[0x3C] = 1;
    auto* parent = asPtr(load32(self, 4));
    const auto player = load32(parent, 0x90);
    if (player == 0) {
        return;
    }
    const auto found = calls.call2(0x008878C0, asPtr(player + 0x394), load32(self, 0x0C),
                                   load32(self, 0x18));
    store32(self, 0x20, found);
    if (found == 0) {
        return;
    }
    store32(self, 0x20, found);
    const auto selfBits = asU(self);
    calls.cdecl2(0x008BC5B0, found + 0x0C, asU(&selfBits));
}

}  // namespace

std::uint32_t buildableUnitPhaseIsNotOne(const std::uint8_t* self) noexcept {
    // 0x008B85F0. xor eax,eax; cmp [ecx+0x18], 1; setne al.
    const auto phase = load32(self, 0x18);
    return phase != 1u ? 1u : 0u;
}

std::uint32_t buildableUnitPhaseIsOne(const std::uint8_t* self) noexcept {
    // 0x008B8600.
    const auto phase = load32(self, 0x18);
    return phase == 1u ? 1u : 0u;
}

std::uint32_t copyBuildableUnitAnchor(const std::uint8_t* self, std::uint8_t* dest) noexcept {
    // 0x008B8610. Three fld/fstp pairs, ret 4. EAX is the destination pointer.
    std::memcpy(dest, self + 0x40, 4);
    std::memcpy(dest + 4, self + 0x44, 4);
    std::memcpy(dest + 8, self + 0x48, 4);
    return asU(dest);
}

std::uint32_t pushAIBuildableStructureRecord(std::uint8_t* self, std::uint8_t* playerObject,
                                              std::uint32_t clearPhase, SkirmishCalls& calls) noexcept {
    // 0x008B8FE0. arg2 == 0 clears the phase selector. The vector is player+0x618 or +0x640.
    if (static_cast<std::uint8_t>(clearPhase) == 0) {
        store32(self, 0x14, 0);
    }
    const auto value = load32(self, 0x2C);
    auto* player = asPtr(load32(playerObject, 0x90));
    auto* vec = player + (load32(self, 0x14) == 0 ? 0x618u : 0x640u);
    const auto end = load32(vec, 4);
    const auto cap = load32(vec, 8);
    if (end < cap) {
        store32(vec, 4, end + 4u);
        if (end != 0) {
            store32(asPtr(end), 0, value);
        }
        return 0;
    }
    const auto slot = value;
    calls.call2(0x008ADD50, vec, end, asU(&slot));
    return 0;
}

std::uint32_t createAIReactiveDefenseTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    return createSized(self, 0xBC, 0x008CC9B0, calls);
}
std::uint32_t createAISimpleDefenseTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    return createSized(self, 0xA0, 0x008CCBC0, calls);
}
std::uint32_t createAIStaticDefenseTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    return createSized(self, 0xB0, 0x008CCC40, calls);
}
std::uint32_t createAIExactExpansionTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    return createSized(self, 0xBC, 0x008CCEE0, calls);
}
std::uint32_t createAISimpleExpansionTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    return createSized(self, 0xB4, 0x008CCF90, calls);
}
std::uint32_t createAIEngineerTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    return createSized(self, 0xA8, 0x008CD050, calls);
}
std::uint32_t createAIGarrisonTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    return createSized(self, 0xA8, 0x008CD440, calls);
}
std::uint32_t createAISimpleAttackTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    return createSized(self, 0xB0, 0x008CD4E0, calls);
}
std::uint32_t createAISimpleSiegeTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    return createSized(self, 0xA0, 0x008CD710, calls);
}
std::uint32_t createAIHuntTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    return createSized(self, 0xA4, 0x008CD7E0, calls);
}
std::uint32_t createAINullTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    return createSized(self, 0xA0, 0x008CD900, calls);
}

std::uint32_t createAIFlankAttackTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008CD3F0. sete cl leaves the rest of the table pointer in ECX, and that
    // whole register is the fourth constructor argument.
    auto* obj = calls.allocate(0xA4);
    if (obj == nullptr) {
        return 0;
    }
    const auto player = load32(self, 0x54);
    const auto ai = load32(self, 0x50);
    const auto table = tacticTable(ai);
    const auto word = tacticWord(ai, player, 4);
    const auto packed = (table & 0xFFFFFF00u) | (word == 2u ? 1u : 0u);
    return calls.construct4(0x008CD140, obj, ai, player, asU(self + 0x38), packed);
}

std::uint32_t createAIScriptedTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008CD9F0. Allocates 0xA0, builds a temporary string, then writes vtable 0x00C535DC.
    auto* obj = calls.allocate(0xA0);
    if (obj == nullptr) {
        return 0;
    }
    const auto player = load32(self, 0x54);
    const auto ai = load32(self, 0x50);
    std::uint32_t temp = 0;
    calls.call1(0x004CBFF0, reinterpret_cast<std::uint8_t*>(&temp), 0x00BD15C0);
    calls.call5(0x008B4900, obj, ai, player, asU(&temp), asU(self + 0x38), 1u);
    calls.call0(0x004CA930, reinterpret_cast<std::uint8_t*>(&temp));
    store32(obj, 0, 0x00C535DC);
    return asU(obj);
}

std::uint32_t markAISimpleDefenseContact(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C1340.
    const auto subject = load32(self, 0x28);
    if (subject != 0) {
        const auto pos = calls.call0(0x0089A740, asPtr(subject));
        calls.call1(0x008B6260, self, pos);
    }
    asPtr(load32(self, 0x2C))[0x10A] = 1;
    return 0;
}

std::uint32_t cacheAIEngineerTacticRecord(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C1370.
    const auto found = calls.call1(0x00885BB0, self + 0x58, 1u);
    store32(self, 0xA0, found);
    calls.call1(0x008B5370, self, 0u);
    return 0;
}

std::uint32_t gateAIScriptedTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C9A70.
    const auto live = calls.call1(0x008818E0, self, 0u);
    if (live == 0 || load32(self, 0x28) == 0) {
        calls.call2(0x008C16A0, self, 0u, 0u);
    }
    return 0;
}

std::uint32_t tryAISimpleDefenseActivation(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C8DA0.
    if (load32(self, 0x28) == 0 || self[0x14] == 0) {
        return 0;
    }
    const auto ready = calls.call0(0x008A8B20, self);
    if ((ready & 0xFFu) == 0) {
        const auto held = calls.call0(0x0089A640, asPtr(load32(self, 0x28)));
        if ((held & 0xFFu) == 0) {
            return 0;
        }
    }
    calls.call2(0x008C16A0, self, 1u, 0u);
    return 0;
}

std::uint32_t cacheAIStaticDefensePosition(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C8DE0.
    const auto live = calls.call1(0x008818E0, self, 0u);
    if (live == 0 || load32(self, 0x28) == 0) {
        calls.call2(0x008C16A0, self, 0u, 0u);
        return 0;
    }
    const auto pos = calls.call0(0x0089A740, asPtr(load32(self, 0x28)));
    std::memcpy(self + 0xA0, asPtr(pos), 12);
    calls.call1(0x008B6260, self, asU(self + 0xA0));
    return 0;
}

std::uint32_t updateAIFlankAttackTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C96F0.
    const auto target = calls.call1(0x008818E0, self, 0u);
    if (target == 0 || load32(self, 0x28) == 0) {
        calls.call2(0x008C16A0, self, 0u, 0u);
        return 0;
    }
    std::uint32_t buffer[3] = {};
    calls.call1(0x007E5660, asPtr(target), asU(buffer));
    const auto pos = calls.call0(0x0089A740, asPtr(load32(self, 0x28)));
    std::uint32_t floats[3] = {};
    std::memcpy(floats, asPtr(pos), 12);
    const auto player = load32(self, 0x54);
    const auto word = tacticWord(load32(self, 0x50), player, 4);
    const auto packed = (player & 0xFFFFFF00u) | (word == 2u ? 1u : 0u);
    calls.call6(0x008B4560, asPtr(load32(self, 0xA0)), load32(self, 0x2C), target, load32(self, 0x28),
                asU(buffer), asU(floats), packed);
    calls.call1(0x008B5370, self, 0u);
    return 0;
}

std::uint32_t updateAIReactiveDefenseTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008CCAA0. The float copy always runs; the fail path is the later null tests.
    const auto live = calls.call1(0x008818E0, self, 0u);
    const auto ai = load32(self, 0x50);
    const auto playerIndex = load32(self, 0x54);
    const auto slot = tacticWord(ai, playerIndex, 0x30);
    auto* player = playerFrom(self);
    const auto found = calls.call2(0x008908C0, player + 0x454, slot, 0u);
    const auto inner = asPtr(load32(player, 0x7DC));
    const auto point = asPtr(load32(inner, 0));
    std::memcpy(self + 0xB4, point + 0x60, 4);
    if (live == 0 || found == 0) {
        calls.call2(0x008C16A0, self, 0u, 0u);
        return 0;
    }
    store32(self, 0xA4, load32(asPtr(found), 4));
    const auto passed = calls.call0(0x00890400, asPtr(found));
    calls.call1(0x008B6260, self, passed);
    const auto cell = calls.call1(0x00625090, player + 0x7FC, 0x00DFC6D4);
    store32(asPtr(cell), 0, 1u);
    return 0;
}

std::uint32_t clearAIReactiveDefenseAlert(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008CCB40.
    auto* owner = asPtr(load32(self, 0x2C));
    auto* map = playerFrom(self) + 0x7FC;
    const auto first = calls.call1(0x00625090, map, 0x00DFC6D4);
    store32(asPtr(first), 0, 0u);
    const auto second = calls.call1(0x00625090, map, 0x00DFC67C);
    store32(asPtr(second), 0, calls.gameFrame + 1u);
    owner[0x10A] = 0;
    return 0;
}

std::uint32_t clearAIHuntAlert(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008CD880.
    if (self[0xA0] != 0) {
        return 0;
    }
    auto* map = playerFrom(self) + 0x7FC;
    const auto first = calls.call1(0x00625090, map, 0x00DFC6D8);
    store32(asPtr(first), 0, 0u);
    const auto second = calls.call1(0x00625090, map, 0x00DFC680);
    store32(asPtr(second), 0, calls.gameFrame + 1u);
    return 0;
}

std::uint32_t aiReactiveDefenseWindowOpen(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008CCA40. ret 4. jg is signed. mov al,1 / xor al,al keeps the high 24 bits.
    auto* player = playerFrom(self);
    const auto first = calls.call1(0x008CB3B0, player, 0x00DFC6D4);
    const auto second = calls.call1(0x008CB3B0, player, 0x00DFC67C);
    if (first != 0 || static_cast<std::int32_t>(second) > static_cast<std::int32_t>(calls.gameFrame)) {
        return second & 0xFFFFFF00u;
    }
    const auto cell = calls.call1(0x00625090, player + 0x7FC, 0x00DFC6D4);
    store32(asPtr(cell), 0, 1u);
    return (cell & 0xFFFFFF00u) | 1u;
}

std::uint32_t aiHuntWindowOpen(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008CD840. Same signed compare, no store. The stack argument is unused.
    auto* player = playerFrom(self);
    const auto first = calls.call1(0x008CB3B0, player, 0x00DFC6D8);
    const auto second = calls.call1(0x008CB3B0, player, 0x00DFC680);
    if (first != 0 || static_cast<std::int32_t>(second) > static_cast<std::int32_t>(calls.gameFrame)) {
        return second & 0xFFFFFF00u;
    }
    return (second & 0xFFFFFF00u) | 1u;
}

std::uint32_t continueAISimpleAttack(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C9950. Tail target of AISimpleSiegeTactic slot 11.
    const auto subject = load32(self, 0x28);
    if (subject != 0) {
        const auto pos = calls.call0(0x0089A6D0, asPtr(subject));
        calls.call2(0x008C1950, self, 0u, pos);
    }
    const auto live = calls.call1(0x008818E0, self, 0u);
    const auto resolved = calls.call0(0x007E6850, asPtr(live));
    calls.call2(0x0088C0E0, self, 1u, resolved);
    return 0;
}

std::uint32_t chooseAIScriptedTacticMode(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C9AA0. Tail target of AIScriptedTactic slot 11.
    const auto mode = tacticWord(load32(self, 0x50), load32(self, 0x54), 8);
    if (mode == 1u) {
        calls.call2(0x008C16A0, self, 1u, 0u);
        return 0;
    }
    if (mode == 0u) {
        const auto pred = calls.call0(0x008B5050, self);
        if ((pred & 0xFFu) != 0) {
            return pred & 0xFFu;
        }
        calls.call2(0x008C16A0, self, 1u, 0u);
        return 0;
    }
    if (mode == 2u) {
        const auto pred = calls.call0(0x008A88C0, self);
        if ((pred & 0xFFu) == 0) {
            return pred & 0xFFFFFF00u;
        }
        calls.call2(0x008C16A0, self, 1u, 0u);
        return 0;
    }
    return mode;
}

std::uint32_t updateAISimpleSiegeTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008CD760. The success arm is a tail jump to 0x008C9950.
    if (self[0x14] == 0) {
        return 0;
    }
    if (calls.call1(0x008818E0, self, 1u) == 0 || calls.call1(0x008818E0, self, 0u) == 0) {
        calls.call2(0x008C16A0, self, 0u, 0u);
        return 0;
    }
    // test al / je skips the fail call and returns; only a non-zero byte tails into 0x008C9950.
    if ((calls.call1(0x008A8A90, self, 0u) & 0xFFu) == 0) {
        return 0;
    }
    return continueAISimpleAttack(self, calls);
}

std::uint32_t updateAIScriptedTactic(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008CD9D0. Tail jump to 0x008C9AA0 unless the subject rejects.
    const auto subject = load32(self, 0x28);
    if (subject != 0) {
        const auto held = calls.call0(0x0089A640, asPtr(subject));
        if ((held & 0xFFu) == 0) {
            return held & 0xFFu;
        }
    }
    return chooseAIScriptedTacticMode(self, calls);
}

std::uint32_t bindAIBuildableUnitTarget(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C4D40.
    bindBuildTarget(self, calls);
    return 0;
}

std::uint32_t bindAIBuildableStructureTarget(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C5140. Walks the catalog list for marker 0xA112A404, then the shared binder.
    // The ledger size stops on the epilogue byte; execution continues through pop ecx / ret
    // and the five-byte island at 0x008C51DD (mov eax, edx; jmp back to the store).
    const auto key = load32(self, 0x10);
    const auto found = calls.call1(0x0078D880, calls.catalog, key);
    if (found != 0) {
        const auto again = calls.call1(0x0078D880, calls.catalog, key);
        auto* node = asPtr(load32(asPtr(again), 0x1A8));
        auto* end = asPtr(load32(asPtr(again), 0x1AC));
        std::uint32_t match = 0;
        if (node != end) {
            for (;;) {
                const auto pointed = load32(node, 4);
                if (load32(asPtr(pointed), 0) == 0xA112A404u) {
                    match = pointed;
                    break;
                }
                node += 0x14;
                if (node == end) {
                    break;
                }
            }
        }
        store32(self, 0x68, match);
    }
    bindBuildTarget(self, calls);
    return 0;
}

void xferSkirmishAISpecialPowerStore(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C0100. Version pair 1,2. Helpers run when the minor byte is >= 2.
    std::uint8_t pair[4] = {};
    versionPair(calls, pair, 1, 2);
    if (pair[1] < 2) {
        return;
    }
    calls.cdecl2(0x008BC0C0, asU(calls.xfer.xfer), asU(self + 0x10));
    calls.cdecl2(0x008BC310, asU(calls.xfer.xfer), asU(self + 0x2C));
}

void xferAiBuildEfficiencyCalculator(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008BDC40.
    std::uint8_t pair[4] = {};
    versionPair(calls, pair, 1, 1);
    calls.cdecl2(0x008BBCE0, asU(calls.xfer.xfer), asU(self + 4));
    calls.cdecl2(0x008A5020, asU(calls.xfer.xfer), asU(self + 0x18));
    calls.xfer.field6c(calls.xfer.xfer, self + 0x28);
}

void xferAINeedsBuilder(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C3040.
    std::uint8_t pair[4] = {};
    versionPair(calls, pair, 1, 1);
    calls.cdecl2(0x0082D820, asU(calls.xfer.xfer), asU(self + 0x2C));
    calls.cdecl2(0x0082D820, asU(calls.xfer.xfer), asU(self + 0x38));
    calls.xfer.field20(calls.xfer.xfer, self + 8);
    versionPair(calls, pair, 1, 1);
    calls.cdecl2(0x0073A470, asU(calls.xfer.xfer), asU(self + 0x1C));
}

void xferAIRecruitmentManagerDataCache(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008B7860. Bit 1 of the xfer flags clears both ranges before the snapshots.
    std::uint8_t pair[4] = {};
    versionPair(calls, pair, 1, 1);
    if (loadFlag(calls)) {
        calls.call2(0x007A7BA0, self + 4, load32(self, 8), load32(self, 0x0C));
        store32(self, 0x10, 0);
        calls.call2(0x0087DF00, self + 0x24, load32(self, 0x28), load32(self, 0x2C));
        store32(self, 0x30, 0);
    }
    calls.cdecl2(0x008A51A0, asU(calls.xfer.xfer), asU(self + 4));
    calls.cdecl2(0x008AD720, asU(calls.xfer.xfer), asU(self + 0x24));
    calls.xfer.field6c(calls.xfer.xfer, self + 0x44);
}

void xferAIUpgradeScience(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C4B10.
    std::uint8_t pair[4] = {};
    versionPair(calls, pair, 1, 1);
    calls.call1(0x00887BE0, self, asU(calls.xfer.xfer));
    calls.xfer.field84(calls.xfer.xfer, self + 0x4C);
    auto first = load32(self, 0x54);
    calls.xfer.field70(calls.xfer.xfer, &first);
    store32(self, 0x54, first);
    auto second = load32(self, 0x58);
    calls.xfer.field70(calls.xfer.xfer, &second);
    store32(self, 0x58, second);
    if (loadFlag(calls)) {
        calls.call1(0x008C4990, self, first);
    }
}

void xferAIUnitUpgrader(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C48E0.
    std::uint8_t pair[4] = {};
    versionPair(calls, pair, 1, 1);
    calls.xfer.field20(calls.xfer.xfer, self + 0x30);
    calls.xfer.field70(calls.xfer.xfer, self + 0x40);
    versionPair(calls, pair, 1, 1);
    calls.cdecl2(0x0073A470, asU(calls.xfer.xfer), asU(self + 8));
    versionPair(calls, pair, 1, 1);
    calls.cdecl2(0x0073A470, asU(calls.xfer.xfer), asU(self + 0x1C));
}

void xferAIOpeningMover(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008BFF40. The second version pair starts at 1,1; minor >= 2 calls 0x0087F630(this-4).
    std::uint8_t first[4] = {};
    versionPair(calls, first, 1, 2);
    calls.xfer.field20(calls.xfer.xfer, self + 4);
    calls.xfer.field70(calls.xfer.xfer, self + 0x18);
    calls.xfer.field84(calls.xfer.xfer, self + 0x1C);
    std::uint8_t second[4] = {};
    versionPair(calls, second, 1, 1);
    calls.cdecl2(0x0073A470, asU(calls.xfer.xfer), asU(self + 0x24));
    // cmp byte [esp+9], 2 / jb reads the first pair. minor < 2 keeps field +0x6C.
    if (first[1] < 2) {
        calls.xfer.field6c(calls.xfer.xfer, self + 0x38);
    } else {
        calls.call1(0x0087F630, self - 4, asU(calls.xfer.xfer));
    }
}

void xferAIRebuildableStructure(std::uint8_t* self, SkirmishCalls& calls) noexcept {
    // 0x008C58F0.
    std::uint8_t pair[4] = {};
    versionPair(calls, pair, 1, 1);
    calls.call1(0x008C5060, self, asU(calls.xfer.xfer));
    std::uint32_t flag = load32(self, 0x70) != 0 ? 1u : 0u;
    calls.xfer.field84(calls.xfer.xfer, &flag);
    if (static_cast<std::uint8_t>(flag) == 0) {
        return;
    }
    std::uint32_t ident = 0xFFFFFFFFu;
    const auto current = load32(self, 0x70);
    if (current != 0) {
        ident = load32(asPtr(current), 4);
    }
    calls.xfer.field88(calls.xfer.xfer, &ident, 0x00C51504, 4u);
    if (loadFlag(calls)) {
        const auto replaced = calls.call1(0x008884E0, asPtr(load32(self, 0x6C)), ident);
        store32(self, 0x70, replaced);
    }
}

}  // namespace ra3::gamelogic
