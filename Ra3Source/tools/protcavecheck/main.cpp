#include "ra3/gamelogic/ProtectedCaveLogic.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {

using ra3::gamelogic::BehaviorModule00C01108Layout;
using ra3::gamelogic::ModuleTreeEntry;
using ra3::gamelogic::SubModuleEntry;
using ra3::gamelogic::UpdateModule00C11AA0Layout;
using ra3::gamelogic::WeaponModifierPair;

struct Buffer {
    std::uint8_t bytes[1024];

    std::uint32_t u32(std::size_t off) const {
        std::uint32_t v = 0;
        std::memcpy(&v, bytes + off, sizeof(v));
        return v;
    }
    std::int32_t i32(std::size_t off) const {
        std::int32_t v = 0;
        std::memcpy(&v, bytes + off, sizeof(v));
        return v;
    }
    float f32(std::size_t off) const {
        float v = 0.0f;
        std::memcpy(&v, bytes + off, sizeof(v));
        return v;
    }
    std::uint8_t u8(std::size_t off) const {
        return bytes[off];
    }
    std::int8_t i8(std::size_t off) const {
        return static_cast<std::int8_t>(bytes[off]);
    }
    void setU32(std::size_t off, std::uint32_t v) {
        std::memcpy(bytes + off, &v, sizeof(v));
    }
    void setI32(std::size_t off, std::int32_t v) {
        std::memcpy(bytes + off, &v, sizeof(v));
    }
    void setF32(std::size_t off, float v) {
        std::memcpy(bytes + off, &v, sizeof(v));
    }
    void setU8(std::size_t off, std::uint8_t v) {
        bytes[off] = v;
    }
};

void dispatch(const std::string& op, Buffer& b) {
    if (op == "ctor_695550") {
        BehaviorModule00C01108Layout layout{};
        for (std::size_t i = 0; i < 19; ++i) {
            layout.words[i] = b.u32(0x100 + i * 4);
        }
        const std::uint32_t ret = ra3::gamelogic::constructBehaviorModule00C01108(
            b.u32(0x00), b.u32(0x04), b.u32(0x08), b.u32(0x0C), layout);
        b.setU32(0x10, ret);
        for (std::size_t i = 0; i < 19; ++i) {
            b.setU32(0x100 + i * 4, layout.words[i]);
        }
    } else if (op == "sub_ctrl_699780") {
        std::int32_t cached = b.i32(0x10);
        const bool ok = ra3::gamelogic::hasActiveSubController(
            b.u32(0x00), b.u8(0x04) != 0U, b.u32(0x08), b.u32(0x0C), cached, b.u32(0x14));
        b.setI32(0x10, cached);
        b.setU32(0x18, ok ? 1U : 0U);
    } else if (op == "part_mod_69e780") {
        const std::int32_t cnt = ra3::gamelogic::computePartitionModifierCount(
            b.u8(0x00) != 0U, b.u8(0x01) != 0U, b.u8(0x02) != 0U,
            b.u32(0x04), b.i32(0x08), b.i32(0x0C),
            b.u8(0x10) != 0U, b.u8(0x11) != 0U, b.f32(0x14));
        b.setI32(0x18, cnt);
    } else if (op == "sub_mod_6a51a0") {
        SubModuleEntry entries[4]{};
        const std::uint32_t count = b.u32(0x04);
        for (std::uint32_t i = 0; i < count && i < 4U; ++i) {
            const std::size_t base = 0x20 + i * 16;
            entries[i].modulePtr = b.u32(base + 0);
            entries[i].tag = b.i32(base + 4);
            entries[i].wildcardPredicateResult = b.u32(base + 8);
            entries[i].interfaceResult = b.u32(base + 12);
        }
        const std::uint32_t res = ra3::gamelogic::queryObjectSubModule(
            b.u8(0x00) != 0U, entries, count < 4U ? count : 4U, b.i32(0x08));
        b.setU32(0x10, res);
    } else if (op == "elig_6a5fa0") {
        const bool ok = ra3::gamelogic::isObjectEligibleForStatusQuery(
            b.u8(0x00), b.u32(0x04), b.u8(0x08), b.u32(0x0C),
            b.u32(0x10), b.u32(0x14), b.u32(0x18),
            b.u8(0x1C), b.u8(0x1D), b.u8(0x1E), b.u8(0x1F));
        b.setU32(0x20, ok ? 1U : 0U);
    } else if (op == "unreg_6a6460") {
        std::uint8_t f4d8 = b.u8(0x00);
        std::uint8_t f4d7 = b.u8(0x01);
        std::uint32_t mask = 0U;
        ra3::gamelogic::unregisterObjectFromSubsystems(
            f4d8, f4d7, b.u8(0x02), b.u8(0x03) != 0U, b.u32(0x04),
            b.u8(0x08) != 0U, b.u8(0x09) != 0U, b.u8(0x0A) != 0U, b.u8(0x0B) != 0U, mask);
        b.setU8(0x00, f4d8);
        b.setU8(0x01, f4d7);
        b.setU32(0x10, mask);
    } else if (op == "pos_delta_6a6dc0") {
        float pos38[3] = {b.f32(0x10), b.f32(0x14), b.f32(0x18)};
        float vel[3] = {b.f32(0x20), b.f32(0x24), b.f32(0x28)};
        float prev21C[3] = {b.f32(0x30), b.f32(0x34), b.f32(0x38)};
        float targ228[3] = {b.f32(0x40), b.f32(0x44), b.f32(0x48)};
        float out[3] = {};
        ra3::gamelogic::updateObjectPositionDelta(
            b.u8(0x00), b.u8(0x01), b.u8(0x02),
            b.u32(0x04), b.u32(0x08),
            pos38, vel, prev21C, targ228, out);
        b.setF32(0x50, out[0]);
        b.setF32(0x54, out[1]);
        b.setF32(0x58, out[2]);
    } else if (op == "ctrl_player_6a7600") {
        const std::uint32_t res = ra3::gamelogic::getObjectControllingPlayer(
            b.u32(0x00), b.u32(0x04), b.u32(0x08), b.u32(0x0C));
        b.setU32(0x10, res);
    } else if (op == "max_health_6a8400") {
        std::uint32_t sel = 0U;
        std::uint32_t boundId = 0U;
        bool disp = false;
        ra3::gamelogic::notifyActiveBodyMaxHealthChanged(
            b.u32(0x00), b.u32(0x04), b.u32(0x08), b.u32(0x0C), b.u32(0x10),
            sel, boundId, disp);
        b.setU32(0x14, sel);
        b.setU32(0x18, boundId);
        b.setU8(0x1C, disp ? 1U : 0U);
    } else if (op == "ai_target_6b7040") {
        std::uint32_t curPtr = b.u32(0x00);
        std::int32_t oldRef = b.i32(0x04);
        bool oldDest = false;
        std::uint8_t f428 = b.u8(0x08);
        std::uint8_t f432 = b.u8(0x09);
        const std::uint32_t newPtr = b.u32(0x0C);
        std::int32_t newRef = b.i32(0x10);
        bool newDest = false;
        bool v270 = false;
        bool h6900 = false;
        ra3::gamelogic::assignAIUpdateTargetRef(
            curPtr, oldRef, oldDest, f428, f432, newPtr, newRef, newDest, v270, h6900);
        b.setU32(0x00, curPtr);
        b.setI32(0x04, oldRef);
        b.setU8(0x08, f428);
        b.setU8(0x09, f432);
        b.setI32(0x10, newRef);
        b.setU8(0x14, oldDest ? 1U : 0U);
        b.setU8(0x15, newDest ? 1U : 0U);
        b.setU8(0x16, v270 ? 1U : 0U);
        b.setU8(0x17, h6900 ? 1U : 0U);
    } else if (op == "classify_6bb360") {
        std::uint32_t mask = 0U;
        ra3::gamelogic::classifyObjectSurfaceAndKindMask(
            b.u32(0x00), b.u32(0x04), b.u32(0x08), b.u32(0x0C),
            b.u32(0x10), b.u32(0x14), b.u32(0x18),
            b.u8(0x1C) != 0U, b.u8(0x1D) != 0U, mask);
        b.setU32(0x20, mask);
    } else if (op == "filter_6bd510") {
        const bool ok = ra3::gamelogic::matchesObjectClassificationFilter(
            b.u32(0x00), b.u32(0x04), b.u32(0x08), b.u32(0x0C),
            b.u32(0x10), b.u32(0x14), b.u32(0x18),
            b.u8(0x1C) != 0U, b.u8(0x1D) != 0U,
            b.u32(0x20), b.u32(0x24));
        b.setU32(0x28, ok ? 1U : 0U);
    } else if (op == "ind_color_6c0620") {
        const std::uint32_t col = ra3::gamelogic::getObjectIndicatorColor(
            b.u32(0x00), b.u32(0x04), b.u32(0x08), b.u32(0x0C),
            b.u8(0x10), b.u32(0x14), b.u32(0x18),
            b.u32(0x1C), b.u32(0x20), b.u32(0x24), b.u32(0x28));
        b.setU32(0x2C, col);
    } else if (op == "bit31_6c0b60") {
        std::uint32_t flagsC0 = b.u32(0x00);
        const std::uint32_t count = b.u32(0x10);
        std::uint32_t listeners[8]{};
        for (std::uint32_t i = 0; i < count && i < 8U; ++i) {
            listeners[i] = b.u32(0x20 + i * 4);
        }
        bool dirty = false;
        std::uint32_t notified = 0U;
        ra3::gamelogic::setObjectBit31AndNotifyModules(
            flagsC0, b.u8(0x04), b.u32(0x08), b.u32(0x0C),
            listeners, count < 8U ? count : 8U, dirty, notified);
        b.setU32(0x00, flagsC0);
        b.setU8(0x14, dirty ? 1U : 0U);
        b.setU32(0x18, notified);
    } else if (op == "find_mod_6c1500") {
        const std::uint32_t count = b.u32(0x0C);
        ModuleTreeEntry entries[8]{};
        for (std::uint32_t i = 0; i < count && i < 8U; ++i) {
            entries[i].key = b.i32(0x20 + i * 8);
            entries[i].modulePtr = b.u32(0x24 + i * 8);
        }
        const std::uint32_t found = ra3::gamelogic::findObjectModuleByKey(
            b.u8(0x00) != 0U, b.u32(0x04), b.u32(0x08),
            entries, count < 8U ? count : 8U, b.i32(0x10));
        b.setU32(0x14, found);
    } else if (op == "reassign_6c1d60") {
        std::uint32_t mask = 0U;
        ra3::gamelogic::reassignObjectControllingPlayer(
            b.u32(0x00), b.u32(0x04), b.u32(0x08), b.u32(0x0C),
            b.u8(0x10) != 0U, mask);
        b.setU32(0x14, mask);
    } else if (op == "shroud_6c2c30") {
        std::uint8_t flags471 = b.u8(0x28);
        const std::uint32_t act = ra3::gamelogic::updateObjectVisionShroudRegistration(
            b.u8(0x00) != 0U, b.u8(0x01) != 0U, b.u8(0x02),
            b.u32(0x04), b.i32(0x08), b.u32(0x0C),
            b.u32(0x10), b.u32(0x14), b.u32(0x18),
            b.u32(0x1C), b.u8(0x20), flags471);
        b.setU8(0x28, flags471);
        b.setU32(0x2C, act);
    } else if (op == "cmd16_6c4f10") {
        float pos[3] = {b.f32(0x00), b.f32(0x04), b.f32(0x08)};
        std::int32_t ref50 = b.i32(0x0C);
        bool refDest = false;
        float outPos[3] = {};
        std::uint32_t finalVt = 0U;
        std::uint32_t freedMask = 0U;
        ra3::gamelogic::dispatchModuleCommandMessage16(
            pos, ref50, refDest, b.u8(0x10) != 0U, b.u8(0x11) != 0U,
            outPos, finalVt, freedMask);
        b.setI32(0x0C, ref50);
        b.setU8(0x12, refDest ? 1U : 0U);
        b.setF32(0x14, outPos[0]);
        b.setF32(0x18, outPos[1]);
        b.setF32(0x1C, outPos[2]);
        b.setU32(0x20, finalVt);
        b.setU32(0x24, freedMask);
    } else if (op == "cmd3d_6c5480") {
        std::int32_t ref50 = b.i32(0x04);
        bool refDest = false;
        std::uint32_t outType = 0U;
        std::uint32_t outArg = 0U;
        std::uint32_t finalVt = 0U;
        std::uint32_t freedMask = 0U;
        ra3::gamelogic::dispatchModuleCommandMessage3D(
            b.u32(0x00), ref50, refDest, b.u8(0x08) != 0U, b.u8(0x09) != 0U,
            outType, outArg, finalVt, freedMask);
        b.setI32(0x04, ref50);
        b.setU8(0x0A, refDest ? 1U : 0U);
        b.setU32(0x0C, outType);
        b.setU32(0x10, outArg);
        b.setU32(0x14, finalVt);
        b.setU32(0x18, freedMask);
    } else if (op == "clear_b15_6c6310") {
        std::uint32_t flagsB0 = b.u32(0x00);
        bool dirty = false;
        const bool ret = ra3::gamelogic::clearObjectStatusBit15AndMarkDirty(flagsB0, dirty);
        b.setU32(0x00, flagsB0);
        b.setU8(0x04, dirty ? 1U : 0U);
        b.setU8(0x05, ret ? 1U : 0U);
    } else if (op == "dtor_6c8380") {
        UpdateModule00C11AA0Layout layout{};
        for (std::size_t i = 0; i < 73; ++i) {
            layout.words[i] = b.u32(0x100 + i * 4);
        }
        std::uint32_t freedMask = 0U;
        bool unlinked = false;
        ra3::gamelogic::destroyUpdateModule00C11AA0(b.u32(0x00), layout, freedMask, unlinked);
        b.setU32(0x04, freedMask);
        b.setU8(0x08, unlinked ? 1U : 0U);
        for (std::size_t i = 0; i < 73; ++i) {
            b.setU32(0x100 + i * 4, layout.words[i]);
        }
    } else if (op == "elig_6cace0") {
        const bool ok = ra3::gamelogic::evaluateBehaviorEligibility00C00F4C(
            b.u8(0x00) != 0U, b.u8(0x01) != 0U, b.u32(0x04), b.u8(0x08),
            b.u8(0x09) != 0U, b.u32(0x0C), b.u8(0x10) != 0U,
            b.u32(0x14), b.u32(0x18), b.u8(0x1C) != 0U,
            b.u8(0x1D) != 0U, b.u8(0x1E) != 0U);
        b.setU32(0x20, ok ? 1U : 0U);
    } else if (op == "max_health_upd_6d0330") {
        std::int32_t val1C = b.i32(0x04);
        const std::uint32_t act = ra3::gamelogic::dispatchActiveBodyMaxHealthUpdate(
            b.i32(0x00), val1C);
        b.setI32(0x04, val1C);
        b.setU32(0x08, act);
    } else if (op == "vis_bcast_6d1dd0") {
        std::uint32_t nMod = 0U;
        bool updShroud = false;
        std::uint32_t n180 = 0U;
        std::uint32_t n17C = 0U;
        bool tail = false;
        ra3::gamelogic::broadcastObjectVisibilityState(
            b.u32(0x00), b.u8(0x04) != 0U, b.u8(0x05) != 0U,
            b.u32(0x08), b.u32(0x0C), b.u32(0x10), b.u32(0x14), b.u32(0x18),
            nMod, updShroud, n180, n17C, tail);
        b.setU32(0x1C, nMod);
        b.setU8(0x20, updShroud ? 1U : 0U);
        b.setU32(0x24, n180);
        b.setU32(0x28, n17C);
        b.setU8(0x2C, tail ? 1U : 0U);
    } else if (op == "lookup44_6e4760") {
        const std::uint32_t count = b.u32(0x14);
        std::uint32_t results[8]{};
        for (std::uint32_t i = 0; i < count && i < 8U; ++i) {
            results[i] = b.u32(0x20 + i * 4);
        }
        std::uint32_t outObj = 0xDEADBEEFU;
        const std::uint32_t iface = ra3::gamelogic::lookupObjectModuleInterface44ById(
            b.u32(0x00), b.u32(0x04), b.u8(0x08), b.u32(0x0C), b.u32(0x10),
            results, count < 8U ? count : 8U, outObj);
        b.setU32(0x18, outObj);
        b.setU32(0x1C, iface);
    } else if (op == "clear_26c_6e5ce0") {
        std::uint32_t w = b.u32(0x00);
        ra3::gamelogic::clearObjectStatusWord0(w);
        b.setU32(0x00, w);
    } else if (op == "refresh_tmpl_6ee7b0") {
        float f0C = b.f32(0x10);
        std::uint32_t f24 = b.u32(0x14);
        bool vt10 = false;
        bool h6d = false;
        ra3::gamelogic::refreshObjectTemplateRegistration(
            b.u32(0x00), b.u32(0x04), b.i32(0x08), b.u32(0x0C),
            f0C, f24, vt10, h6d);
        b.setF32(0x10, f0C);
        b.setU32(0x14, f24);
        b.setU8(0x18, vt10 ? 1U : 0U);
        b.setU8(0x19, h6d ? 1U : 0U);
    } else if (op == "producer_6efe40") {
        const std::uint32_t res = ra3::gamelogic::lookupObjectProducerById(
            b.u32(0x00), b.u32(0x04));
        b.setU32(0x08, res);
    } else if (op == "selectable_6f1350") {
        const bool ok = ra3::gamelogic::isObjectSelectableByLocalPlayer(
            b.u32(0x00), b.u32(0x04), b.u32(0x08), b.u8(0x0C),
            b.u8(0x0D) != 0U, b.u8(0x0E) != 0U, b.u32(0x10),
            b.u8(0x14) != 0U, b.u8(0x15) != 0U, b.u8(0x16) != 0U,
            b.u8(0x17) != 0U, b.u8(0x18) != 0U, b.i32(0x1C),
            b.u8(0x20) != 0U, b.u8(0x21) != 0U, b.u8(0x22) != 0U, b.u8(0x23) != 0U);
        b.setU32(0x24, ok ? 1U : 0U);
    } else if (op == "tint_72cf30") {
        float tint[3] = {b.f32(0x04), b.f32(0x08), b.f32(0x0C)};
        float outTint[3] = {};
        std::uint32_t cmd = 0U;
        const bool ok = ra3::gamelogic::applyBehaviorDrawableTintEffect(
            b.u8(0x00) != 0U, b.u8(0x01) != 0U, tint, outTint, cmd);
        b.setU8(0x10, ok ? 1U : 0U);
        b.setF32(0x14, outTint[0]);
        b.setF32(0x18, outTint[1]);
        b.setF32(0x1C, outTint[2]);
        b.setU32(0x20, cmd);
    } else if (op == "reg_mod_731530") {
        std::uint8_t reg138 = b.u8(0x00);
        std::uint32_t globalCnt = b.u32(0x04);
        std::uint32_t listCnt = b.u32(0x08);
        const std::uint32_t listCap = b.u32(0x0C);
        const std::uint32_t listenerPtr = b.u32(0x10);
        std::uint8_t flag65 = b.u8(0x14);
        const std::uint32_t type0C = b.u32(0x18);
        const std::uint32_t subE0 = b.u32(0x1C);
        std::int32_t cnt1318 = b.i32(0x20);
        std::uint32_t arr[8]{};
        for (std::uint32_t i = 0; i < listCnt && i < 8U; ++i) {
            arr[i] = b.u32(0x30 + i * 4);
        }
        ra3::gamelogic::registerActiveModuleAndAppendListener(
            reg138, globalCnt, arr, listCnt, listCap < 8U ? listCap : 8U,
            listenerPtr, flag65, type0C, subE0, cnt1318);
        b.setU8(0x00, reg138);
        b.setU32(0x04, globalCnt);
        b.setU32(0x08, listCnt);
        b.setU8(0x14, flag65);
        b.setI32(0x20, cnt1318);
        for (std::uint32_t i = 0; i < listCnt && i < 8U; ++i) {
            b.setU32(0x30 + i * 4, arr[i]);
        }
    } else if (op == "gather_wpn_734490") {
        std::uint32_t slots[6]{};
        for (std::size_t i = 0; i < 6; ++i) {
            slots[i] = b.u32(0x04 + i * 4);
        }
        std::uint32_t outVec[8]{};
        std::uint32_t outCnt = 0U;
        const bool ok = ra3::gamelogic::gatherActiveWeaponSlots(
            b.i8(0x00), slots, outVec, outCnt, 8U);
        b.setU8(0x1C, ok ? 1U : 0U);
        b.setU32(0x20, outCnt);
        for (std::uint32_t i = 0; i < outCnt && i < 8U; ++i) {
            b.setU32(0x24 + i * 4, outVec[i]);
        }
    } else if (op == "ctor_73c740") {
        UpdateModule00C11AA0Layout layout{};
        for (std::size_t i = 0; i < 73; ++i) {
            layout.words[i] = b.u32(0x100 + i * 4);
        }
        bool h6906 = false;
        bool h7221 = false;
        const std::uint32_t ret = ra3::gamelogic::constructUpdateModule00C11AA0(
            b.u32(0x00), b.u32(0x04), b.u32(0x08), b.u32(0x0C),
            b.f32(0x10), b.f32(0x14), layout, h6906, h7221);
        b.setU32(0x18, ret);
        b.setU8(0x1C, h6906 ? 1U : 0U);
        b.setU8(0x1D, h7221 ? 1U : 0U);
        for (std::size_t i = 0; i < 73; ++i) {
            b.setU32(0x100 + i * 4, layout.words[i]);
        }
    } else if (op == "wpn_cache_741140") {
        const std::uint32_t inCnt = b.u32(0x04);
        std::uint32_t cacheCnt = b.u32(0x08);
        WeaponModifierPair incoming[8]{};
        WeaponModifierPair cache[8]{};
        for (std::uint32_t i = 0; i < inCnt && i < 8U; ++i) {
            incoming[i].itemPtr = b.u32(0x20 + i * 8);
            incoming[i].value = b.f32(0x24 + i * 8);
        }
        for (std::uint32_t i = 0; i < cacheCnt && i < 8U; ++i) {
            cache[i].itemPtr = b.u32(0x60 + i * 8);
            cache[i].value = b.f32(0x64 + i * 8);
        }
        const std::uint32_t ret = ra3::gamelogic::ensureObjectWeaponModifierCache(
            b.u32(0x00), incoming, inCnt < 8U ? inCnt : 8U, cache, cacheCnt, 8U);
        b.setU32(0x08, cacheCnt);
        b.setU32(0x0C, ret);
        for (std::uint32_t i = 0; i < cacheCnt && i < 8U; ++i) {
            b.setU32(0x60 + i * 8, cache[i].itemPtr);
            b.setF32(0x64 + i * 8, cache[i].value);
        }
    } else if (op == "six_buckets_7451e0") {
        std::uint32_t storage[6][4]{};
        const std::uint32_t* ptrs[6]{};
        std::size_t counts[6]{};
        for (std::size_t bucket = 0; bucket < 6; ++bucket) {
            const std::uint32_t c = b.u32(bucket * 4);
            counts[bucket] = c < 4U ? c : 4U;
            for (std::size_t j = 0; j < counts[bucket]; ++j) {
                storage[bucket][j] = b.u32(0x20 + (bucket * 4 + j) * 4);
            }
            ptrs[bucket] = storage[bucket];
        }
        std::uint32_t outObjs[24]{};
        std::uint32_t outCnt = 0U;
        ra3::gamelogic::notifySixBucketObjectListeners(ptrs, counts, outObjs, outCnt, 24U);
        b.setU32(0x80, outCnt);
        for (std::uint32_t i = 0; i < outCnt && i < 24U; ++i) {
            b.setU32(0x84 + i * 4, outObjs[i]);
        }
    } else if (op == "xform_7461c0") {
        float src[12]{};
        float dst[12]{};
        for (std::size_t i = 0; i < 12; ++i) {
            src[i] = b.f32(0x10 + i * 4);
            dst[i] = b.f32(0x40 + i * 4);
        }
        const bool copied = ra3::gamelogic::updateBehaviorTransformMatrix(
            b.u32(0x00), b.u8(0x04), b.f32(0x08), src, dst);
        b.setU8(0x0C, copied ? 1U : 0U);
        for (std::size_t i = 0; i < 12; ++i) {
            b.setF32(0x40 + i * 4, dst[i]);
        }
    } else if (op == "team_xfer_747920") {
        std::int32_t oldCnt = b.i32(0x18);
        std::int32_t newCnt = b.i32(0x1C);
        bool rem = false;
        bool add = false;
        bool c736 = false;
        ra3::gamelogic::transferObjectTeamListMembership(
            b.u32(0x00), b.u32(0x04), b.u32(0x08), b.u32(0x0C), b.u32(0x10),
            b.u8(0x14) != 0U, b.u8(0x15) != 0U, b.u8(0x16) != 0U, b.u8(0x17) != 0U,
            oldCnt, newCnt, rem, add, c736);
        b.setI32(0x18, oldCnt);
        b.setI32(0x1C, newCnt);
        b.setU8(0x20, rem ? 1U : 0U);
        b.setU8(0x21, add ? 1U : 0U);
        b.setU8(0x22, c736 ? 1U : 0U);
    } else if (op == "ai_idle_747f10") {
        const bool handled = ra3::gamelogic::checkAndHandleObjectAIIdleState(
            b.u8(0x00) != 0U, b.u8(0x01) != 0U, b.u8(0x02),
            b.u32(0x04), b.u8(0x08), b.u8(0x09) != 0U, b.u8(0x0A), b.u8(0x0B) != 0U);
        b.setU32(0x10, handled ? 1U : 0U);
    } else {
        throw std::runtime_error("Unknown op: " + op);
    }
}

Buffer fromHex(const std::string& text) {
    if (text.size() != sizeof(Buffer::bytes) * 2U) {
        throw std::runtime_error("unexpected scenario hex length");
    }
    auto nibble = [](char c) -> std::uint8_t {
        if (c >= '0' && c <= '9') return static_cast<std::uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<std::uint8_t>(c - 'a' + 10);
        if (c >= 'A' && c <= 'F') return static_cast<std::uint8_t>(c - 'A' + 10);
        throw std::runtime_error("invalid hex char");
    };
    Buffer out{};
    for (std::size_t i = 0; i < sizeof(Buffer::bytes); ++i) {
        out.bytes[i] = static_cast<std::uint8_t>(
            (nibble(text[i * 2U]) << 4U) | nibble(text[i * 2U + 1U]));
    }
    return out;
}

void printHex(const Buffer& b) {
    static const char kHex[] = "0123456789abcdef";
    char line[sizeof(Buffer::bytes) * 2U + 2U];
    for (std::size_t i = 0; i < sizeof(Buffer::bytes); ++i) {
        line[i * 2U] = kHex[(b.bytes[i] >> 4U) & 0x0FU];
        line[i * 2U + 1U] = kHex[b.bytes[i] & 0x0FU];
    }
    line[sizeof(Buffer::bytes) * 2U] = '\n';
    line[sizeof(Buffer::bytes) * 2U + 1U] = '\0';
    std::cout.write(line, sizeof(Buffer::bytes) * 2U + 1U);
}

void mutateBuffer(Buffer& b) {
    b.bytes[0x10] ^= 0x01U;
    b.bytes[0x14] ^= 0x01U;
    b.bytes[0x18] ^= 0x01U;
    b.bytes[0x20] ^= 0x01U;
}

}  // namespace

int main(int argc, char** argv) {
    bool forceMutate = false;
    if (argc >= 2 && std::string(argv[1]) == "--mutate") {
        forceMutate = true;
    } else if (argc >= 2) {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif
        const std::string op = argv[1];
        Buffer buf{};
        if (std::fread(buf.bytes, 1, sizeof(buf.bytes), stdin) != sizeof(buf.bytes)) {
            return 3;
        }
        dispatch(op, buf);
        if (std::fwrite(buf.bytes, 1, sizeof(buf.bytes), stdout) != sizeof(buf.bytes)) {
            return 4;
        }
        return 0;
    }

    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (line == "quit") {
            return 0;
        }
        std::istringstream iss(line);
        std::string first;
        iss >> first;
        bool doMutate = forceMutate;
        std::string op;
        std::string hexText;
        if (first == "eval" || first == "mutate") {
            if (first == "mutate") {
                doMutate = true;
            }
            iss >> op >> hexText;
        } else {
            op = first;
            iss >> hexText;
        }
        Buffer buf = fromHex(hexText);
        dispatch(op, buf);
        if (doMutate) {
            mutateBuffer(buf);
        }
        printHex(buf);
        std::cout.flush();
    }
    return 0;
}
