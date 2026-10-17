#include "ra3/gamelogic/SkirmishAIBaseBuild.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

using namespace ra3::gamelogic::skirmishai_base_build;

std::int32_t parseInt(const char* text) {
    return static_cast<std::int32_t>(std::strtol(text, nullptr, 0));
}

std::uint32_t parseUint(const char* text) {
    return static_cast<std::uint32_t>(std::strtoul(text, nullptr, 0));
}

float parseFloat(const char* text) {
    if (std::strncmp(text, "0x", 2) == 0 || std::strncmp(text, "0X", 2) == 0) {
        const std::uint32_t bits = parseUint(text);
        float value = 0.0f;
        std::memcpy(&value, &bits, sizeof(float));
        return value;
    }
    return std::strtof(text, nullptr);
}

std::string hex32(std::uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << value;
    return out.str();
}

std::string floatBitsHex(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return hex32(bits);
}

std::uint32_t readU32(const void* base, std::ptrdiff_t offset) {
    std::uint32_t v = 0;
    std::memcpy(&v, static_cast<const std::uint8_t*>(base) + offset, sizeof(v));
    return v;
}

std::int32_t readI32(const void* base, std::ptrdiff_t offset) {
    std::int32_t v = 0;
    std::memcpy(&v, static_cast<const std::uint8_t*>(base) + offset, sizeof(v));
    return v;
}

std::uint8_t readU8(const void* base, std::ptrdiff_t offset) {
    return *(static_cast<const std::uint8_t*>(base) + offset);
}

void writeU32(void* base, std::ptrdiff_t offset, std::uint32_t v) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &v, sizeof(v));
}

void writeI32(void* base, std::ptrdiff_t offset, std::int32_t v) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &v, sizeof(v));
}

void writeF32(void* base, std::ptrdiff_t offset, float v) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &v, sizeof(v));
}

void writeU8(void* base, std::ptrdiff_t offset, std::uint8_t v) {
    *(static_cast<std::uint8_t*>(base) + offset) = v;
}

template <typename T>
void writePtr(void* base, std::ptrdiff_t offset, T* ptr) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &ptr, sizeof(ptr));
}

struct FakeGameLogicHash {
    std::uint8_t gameLogic[0x200]{};
    void* buckets[5]{};
    std::uint8_t node0[0x10]{};
    std::uint8_t node1[0x10]{};
    std::uint8_t sentinel[0x10]{};

    FakeGameLogicHash() {
        for (int i = 0; i < 4; ++i) buckets[i] = nullptr;
        buckets[4] = sentinel;
        writePtr(gameLogic, 0xC4 + 0x04, buckets);
        writeU32(gameLogic, 0xC4 + 0x08, 4u);
    }

    void addObject(int idx, std::uint32_t key, void* objPtr) {
        std::uint8_t* n = (idx == 0) ? node0 : node1;
        writeU32(n, 0x00, key);
        writePtr(n, 0x04, objPtr);
        const std::uint32_t b = key % 4u;
        writePtr(n, 0x08, buckets[b]);
        buckets[b] = n;
    }
};

struct FakeThingFactoryHash {
    std::uint8_t factory[0x60]{};
    void* buckets[5]{};
    std::uint8_t node0[0x10]{};
    std::uint8_t node1[0x10]{};
    std::uint8_t node2[0x10]{};
    std::uint8_t sentinel[0x10]{};

    FakeThingFactoryHash() {
        for (int i = 0; i < 4; ++i) buckets[i] = nullptr;
        buckets[4] = sentinel;
        writePtr(factory, 0x28 + 0x04, buckets);
        writeU32(factory, 0x28 + 0x08, 4u);
    }

    void addTemplate(int idx, std::uint32_t key, void* tmplPtr) {
        std::uint8_t* n = (idx == 0) ? node0 : (idx == 1) ? node1 : node2;
        writeU32(n, 0x00, key);
        writePtr(n, 0x04, tmplPtr);
        const std::uint32_t b = key % 4u;
        writePtr(n, 0x08, buckets[b]);
        buckets[b] = n;
    }
};

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: ra3_skirmishaibasebuildcheck <op> [args...]\n";
        return 2;
    }

    const std::string op = argv[1];

    // 1. 0x00881130 normalizeBudgetStateScores_881130
    // args: has0 s0 has1 s1 has2 s2
    if (op == "normalize_budget_881130" && argc == 8) {
        AIBudgetStateEntryView entries[3]{};
        int dummy[3] = { 1, 2, 3 };
        for (int i = 0; i < 3; ++i) {
            entries[i].stateAt0 = parseInt(argv[2 + i * 2]) ? &dummy[i] : nullptr;
            entries[i].scoreAt4 = parseFloat(argv[3 + i * 2]);
        }
        AIBudgetStateManagerView mgr{};
        mgr.beginAt4 = entries;
        mgr.endAt8 = entries + 3;
        mgr.capAtC = entries + 3;
        mgr.hasPositiveScoreAt14 = 0xAAu;
        normalizeBudgetStateScores_881130(&mgr);
        std::cout << "pos=" << static_cast<unsigned>(mgr.hasPositiveScoreAt14)
                  << " s0=" << floatBitsHex(entries[0].scoreAt4)
                  << " s1=" << floatBitsHex(entries[1].scoreAt4)
                  << " s2=" << floatBitsHex(entries[2].scoreAt4) << "\n";
        return 0;
    }

    // 2. 0x00882190 resetBuildPosition_882190
    // args: x y z
    if (op == "reset_build_pos_882190" && argc == 5) {
        Coord3DView outPos{ parseFloat(argv[2]), parseFloat(argv[3]), parseFloat(argv[4]) };
        int dummySelf = 0;
        resetBuildPosition_882190(&dummySelf, &outPos);
        std::cout << "x=" << floatBitsHex(outPos.x)
                  << " y=" << floatBitsHex(outPos.y)
                  << " z=" << floatBitsHex(outPos.z) << "\n";
        return 0;
    }

    // 3. 0x00882400 setBuildPositionWithTerrainHeight_882400
    // args: px py pz terrainBaseZ
    if (op == "set_build_pos_terrain_882400" && argc == 6) {
        static float s_baseZ = 0.0f;
        s_baseZ = parseFloat(argv[5]);
        struct FakeTerrain {
            static float __fastcall getGroundHeight(void*, void*, float x, float y, void*) {
                return s_baseZ + x * 0.5f - y * 0.25f;
            }
        };
        void* vtbl[8]{};
        vtbl[6] = reinterpret_cast<void*>(&FakeTerrain::getGroundHeight);
        void* terrainObj = vtbl;

        Coord3DView dst{ -1.0f, -1.0f, -1.0f };
        Coord3DView src{ parseFloat(argv[2]), parseFloat(argv[3]), parseFloat(argv[4]) };
        alignas(16) std::uint8_t self[0x70]{};
        writePtr(self, 0x44, &dst);
        writeU8(self, 0x64, 0u);

        setBuildPositionWithTerrainHeight_882400(self, &src, &terrainObj);
        std::cout << "flag=" << static_cast<unsigned>(readU8(self, 0x64))
                  << " x=" << floatBitsHex(dst.x)
                  << " y=" << floatBitsHex(dst.y)
                  << " z=" << floatBitsHex(dst.z) << "\n";
        return 0;
    }

    // 4/5/6. 0x00882A90, 0x00882AE0, 0x00882C30
    // args: bitIndex count m0 v0 m1 v1
    if ((op == "find_bitmask_zero_882a90" || op == "find_entry20_bit_882ae0" || op == "find_bitmask_one_882c30") && argc == 8) {
        const std::uint32_t bitIndex = parseUint(argv[2]);
        const std::uint32_t count = parseUint(argv[3]);
        alignas(16) std::uint8_t inner[0x150]{};

        if (op == "find_entry20_bit_882ae0") {
            alignas(16) std::uint8_t entries[3 * 20]{};
            for (int i = 0; i < 2; ++i) {
                writeU32(entries, i * 20 + 0x00, static_cast<std::uint32_t>(100 + i));
                writeU32(entries, i * 20 + 0x10, parseUint(argv[4 + i * 2]));
                writeU32(entries, i * 20 + 0x14, parseUint(argv[5 + i * 2]));
            }
            writeU32(inner, 0x10C, count);
            writePtr(inner, 0x110, entries);
            const void* wrapper = inner;
            const void* r = findEntry20ByBit_882AE0(&wrapper, bitIndex);
            const int idx = r ? static_cast<int>((static_cast<const std::uint8_t*>(r) - entries) / 20) : -1;
            std::cout << "idx=" << idx << "\n";
        } else {
            alignas(16) std::uint8_t entries[3 * 12]{};
            for (int i = 0; i < 2; ++i) {
                writeF32(entries, i * 8 + 0x00, parseFloat(argv[5 + i * 2]));
                writeU32(entries, i * 8 + 0x04, parseUint(argv[4 + i * 2]));
            }
            writeU32(inner, 0x104, count);
            writePtr(inner, 0x108, entries);
            writeU32(inner, 0x12C, count);
            writePtr(inner, 0x130, entries);
            const void* wrapper = inner;
            const float r = (op == "find_bitmask_zero_882a90")
                ? findBitmaskFloatOrZero_882A90(&wrapper, bitIndex)
                : findBitmaskFloatOrOne_882C30(&wrapper, bitIndex);
            std::cout << "ret=" << floatBitsHex(r) << "\n";
        }
        return 0;
    }

    // 7. 0x00884550 updateBasePriorityFlags_884550
    // args: count id0 fl0 id1 fl1 id2 fl2
    if (op == "update_base_priority_884550" && argc == 9) {
        const int count = parseInt(argv[2]);
        AIBaseView b[3]{};
        for (int i = 0; i < 3; ++i) {
            b[i].baseIdAt4 = parseInt(argv[3 + i * 2]);
            b[i].flagsAt2C = parseUint(argv[4 + i * 2]);
        }
        AIBaseView* ptrs[3] = { &b[0], &b[1], &b[2] };
        alignas(16) std::uint8_t self[0x40]{};
        writePtr(self, 0x28, ptrs);
        writePtr(self, 0x2C, ptrs + count);
        updateBasePriorityFlags_884550(self);
        std::cout << "f0=" << hex32(b[0].flagsAt2C)
                  << " f1=" << hex32(b[1].flagsAt2C)
                  << " f2=" << hex32(b[2].flagsAt2C) << "\n";
        return 0;
    }

    // 8. 0x00884C40 hasActiveBuilderAtPosition_884C40
    // args: count qx qy b0_act b0_hasTask b0_match14 b0_match10
    if (op == "has_active_builder_884c40" && argc == 9) {
        const int count = parseInt(argv[2]);
        const Coord3DView qpos{ parseFloat(argv[3]), parseFloat(argv[4]), 0.0f };

        static alignas(16) std::uint8_t s_task0[0x20]{};
        static alignas(16) std::uint8_t s_pos14[0x10]{};
        static alignas(16) std::uint8_t s_pos10[0x10]{};
        std::memset(s_task0, 0, sizeof(s_task0));
        std::memset(s_pos14, 0, sizeof(s_pos14));
        std::memset(s_pos10, 0, sizeof(s_pos10));

        writeF32(s_pos14, 0x04, parseInt(argv[7]) ? qpos.x : qpos.x + 99.0f);
        writeF32(s_pos14, 0x08, parseInt(argv[7]) ? qpos.y : qpos.y + 99.0f);
        writeF32(s_pos10, 0x04, parseInt(argv[8]) ? qpos.x : qpos.x + 99.0f);
        writeF32(s_pos10, 0x08, parseInt(argv[8]) ? qpos.y : qpos.y + 99.0f);
        writePtr(s_task0, 0x14, s_pos14);
        writePtr(s_task0, 0x10, s_pos10);

        static int s_hasTask = 0;
        s_hasTask = parseInt(argv[6]);
        struct FakeBuilder {
            static const void* __fastcall getTask(void*, void*) {
                return s_hasTask ? s_task0 : nullptr;
            }
        };
        void* vtbl[32]{};
        vtbl[29] = reinterpret_cast<void*>(&FakeBuilder::getTask);
        void* builderObj = vtbl;

        alignas(16) std::uint8_t vecEntries[2 * 12]{};
        writePtr(vecEntries, 0x00, &builderObj);
        writeU32(vecEntries, 0x04, parseUint(argv[5]));

        alignas(16) std::uint8_t aiPlayer[0x420]{};
        writePtr(aiPlayer, 0x3EC + 0x0C, vecEntries);
        writePtr(aiPlayer, 0x3EC + 0x10, vecEntries + count * 12);

        alignas(16) std::uint8_t ctx[0xA0]{};
        writePtr(ctx, 0x90, aiPlayer);

        alignas(16) std::uint8_t self[0x270]{};
        writePtr(self, 0x264, ctx);

        const bool r = hasActiveBuilderAtPosition_884C40(self, &qpos);
        std::cout << "ret=" << (r ? 1 : 0) << "\n";
        return 0;
    }

    // 9. 0x00886320 evaluateHeuristicProduct_886320
    // args: count v0 v1
    if (op == "eval_heuristic_prod_886320" && argc == 5) {
        const int count = parseInt(argv[2]);
        static float s_v0 = 0.0f, s_v1 = 0.0f;
        s_v0 = parseFloat(argv[3]);
        s_v1 = parseFloat(argv[4]);

        struct FakeHeur {
            static float __fastcall eval0(void*, void*, void*, void*) { return s_v0; }
            static float __fastcall eval1(void*, void*, void*, void*) { return s_v1; }
        };
        void* vt0[2] = { nullptr, reinterpret_cast<void*>(&FakeHeur::eval0) };
        void* vt1[2] = { nullptr, reinterpret_cast<void*>(&FakeHeur::eval1) };
        void* h0 = vt0;
        void* h1 = vt1;

        AIBudgetStateView state{};
        AIHeuristicCallNodeView n0{}, n1{};
        auto* sentinel = reinterpret_cast<AIHeuristicCallNodeView*>(
            reinterpret_cast<std::uint8_t*>(&state) + 0x04);
        if (count == 0) {
            state.nextAt4 = sentinel;
            state.prevAt8 = sentinel;
        } else if (count == 1) {
            state.nextAt4 = &n0;
            n0.nextAt0 = sentinel;
            n0.heuristicAt8 = &h0;
        } else {
            state.nextAt4 = &n0;
            n0.nextAt0 = &n1;
            n0.heuristicAt8 = &h0;
            n1.nextAt0 = sentinel;
            n1.heuristicAt8 = &h1;
        }

        const float r = evaluateHeuristicProduct_886320(&state, nullptr, nullptr);
        std::cout << "ret=" << floatBitsHex(r) << "\n";
        return 0;
    }

    // 10. 0x00887170 popNextTemplateId_887170
    // args: enabled exhausted cursorIdx k0 v0 k1 v1
    if (op == "pop_next_tid_887170" && argc == 9) {
        std::uint32_t pairs[4] = {
            parseUint(argv[5]), parseUint(argv[6]),
            parseUint(argv[7]), parseUint(argv[8])
        };
        const std::uint32_t cursorIdx = parseUint(argv[4]);
        AIBuildableQueueView q{};
        q.enabledAt0 = parseUint(argv[2]);
        q.exhaustedAt0C = static_cast<std::uint8_t>(parseUint(argv[3]));
        q.baseAt8 = pairs;
        q.cursorAt4 = pairs + cursorIdx * 2u;

        const std::uint32_t r = popNextTemplateId_887170(&q);
        const int newIdx = static_cast<int>((q.cursorAt4 - pairs) / 2);
        std::cout << "ret=" << hex32(r)
                  << " idx=" << newIdx
                  << " exh=" << static_cast<unsigned>(q.exhaustedAt0C) << "\n";
        return 0;
    }

    // 11. 0x00887300 findEntryByTemplateId_887300
    // args: count target t0 t1 t2
    if (op == "find_entry_tid_887300" && argc == 7) {
        const std::uint32_t count = parseUint(argv[2]);
        const std::uint32_t target = parseUint(argv[3]);
        alignas(16) std::uint8_t entries[3 * 0x2CC]{};
        for (int i = 0; i < 3; ++i) {
            writeU32(entries, i * 0x2CC + 0x24, parseUint(argv[4 + i]));
        }
        alignas(16) std::uint8_t self[0x30]{};
        writePtr(self, 0x1C, entries);
        writeU32(self, 0x20, count);
        void* r = findEntryByTemplateId_887300(self, target);
        const int idx = r ? static_cast<int>((static_cast<std::uint8_t*>(r) - entries) / 0x2CC) : -1;
        std::cout << "idx=" << idx << "\n";
        return 0;
    }

    // 12. 0x008878C0 lookupNestedValue_8878C0
    // args: outerKey innerKey kOuter kInner0 vInner0 kInner1 vInner1
    if (op == "lookup_nested_8878c0" && argc == 9) {
        alignas(16) std::uint8_t self[0x30]{};
        alignas(16) std::uint8_t outerNode[0x40]{};
        alignas(16) std::uint8_t innerN0[0x30]{};
        alignas(16) std::uint8_t innerN1[0x30]{};

        writeI32(innerN0, 0x10, parseInt(argv[5]));
        writeU32(innerN0, 0x14, parseUint(argv[6]));

        writePtr(innerN1, 0x04, innerN0);
        writeI32(innerN1, 0x10, parseInt(argv[7]));
        writeU32(innerN1, 0x14, parseUint(argv[8]));

        writeI32(outerNode, 0x10, parseInt(argv[4]));
        writePtr(outerNode, 0x14 + 0x0C, innerN1);

        writePtr(self, 0x04 + 0x0C, outerNode);
        const std::uint32_t r = lookupNestedValue_8878C0(self, parseInt(argv[2]), parseInt(argv[3]));
        std::cout << "ret=" << r << "\n";
        return 0;
    }

    // 13. 0x00887D60 initBuildableEntry_887D60
    // args: owner50 tmpl54
    if (op == "init_buildable_entry_887d60" && argc == 4) {
        alignas(16) std::uint8_t self[0x60]{};
        std::memset(self, 0xCC, sizeof(self));
        void* r = initBuildableEntry_887D60(self, parseUint(argv[2]), parseUint(argv[3]));
        std::cout << "ret_eq=" << (r == self ? 1 : 0)
                  << " vft=" << hex32(readU32(self, 0x00))
                  << " f2c=" << hex32(readU32(self, 0x2C))
                  << " f38=" << static_cast<unsigned>(readU8(self, 0x38))
                  << " f40=" << readU32(self, 0x40)
                  << " f4c=" << static_cast<unsigned>(readU8(self, 0x4C))
                  << " f4d=" << static_cast<unsigned>(readU8(self, 0x4D))
                  << " f50=" << readU32(self, 0x50)
                  << " f54=" << readU32(self, 0x54) << "\n";
        return 0;
    }

    // 14. 0x0088A0E0 sumWeightedRequestsAbovePriority_88A0E0 & 25. 0x008900B0 canAffordRequest_8900B0
    // args: count minPrio inOutMax cost rateScale w0C spent1C avail20 p0 c0 p1 c1
    if ((op == "sum_weighted_88a0e0" || op == "can_afford_8900b0") && argc == 14) {
        const int count = parseInt(argv[2]);
        const float minPrio = parseFloat(argv[3]);
        float inOutMax = parseFloat(argv[4]);
        const float reqCost = parseFloat(argv[5]);
        const float rateScale = parseFloat(argv[6]);

        AIBankAccountView acc{};
        acc.budgetWeightAt0C = parseFloat(argv[7]);
        acc.spentAt1C = parseFloat(argv[8]);
        acc.availableAt20 = parseFloat(argv[9]);

        AIBankRequestNodeView n0{}, n1{};
        n0.priorityAt14 = parseFloat(argv[10]);
        n0.costAt18 = parseFloat(argv[11]);
        n1.priorityAt14 = parseFloat(argv[12]);
        n1.costAt18 = parseFloat(argv[13]);

        auto* sentinel = reinterpret_cast<AIBankRequestNodeView*>(
            reinterpret_cast<std::uint8_t*>(&acc) + 0x10);
        if (count == 0) {
            acc.requestListNextAt10 = sentinel;
            acc.requestListPrevAt14 = sentinel;
        } else if (count == 1) {
            acc.requestListNextAt10 = &n0;
            acc.requestListPrevAt14 = &n0;
            n0.nextAt0 = sentinel;
            n0.prevAt4 = sentinel;
        } else {
            acc.requestListNextAt10 = &n0;
            acc.requestListPrevAt14 = &n1;
            n0.nextAt0 = &n1;
            n0.prevAt4 = sentinel;
            n1.nextAt0 = sentinel;
            n1.prevAt4 = &n0;
        }

        if (op == "sum_weighted_88a0e0") {
            const float r = sumWeightedRequestsAbovePriority_88A0E0(&acc, minPrio, &inOutMax);
            std::cout << "ret=" << floatBitsHex(r) << " max=" << floatBitsHex(inOutMax) << "\n";
        } else {
            const bool r = canAffordRequest_8900B0(&acc, reqCost, minPrio, rateScale);
            std::cout << "ret=" << (r ? 1 : 0) << "\n";
        }
        return 0;
    }

    // 15. 0x0088AD80, 16. 0x0088AE10, 17. 0x0088AFC0
    // args: count targetTid builderId e0_bid e0_tid e0_st e0_pwr e1_bid e1_tid e1_st e1_pwr
    if ((op == "sum_queued_power_88ad80" || op == "count_queued_tid_88ae10" || op == "notify_constructed_88afc0") && argc == 13) {
        const int count = parseInt(argv[2]);
        const std::uint32_t targetTid = parseUint(argv[3]);
        const std::uint32_t builderId = parseUint(argv[4]);

        static int s_notified = 0;
        s_notified = 0;
        struct FakeEntry {
            static void __fastcall onNotify0(void*, void*, void* ctx) {
                s_notified = (ctx != nullptr) ? 10 : 1;
            }
            static void __fastcall onNotify1(void*, void*, void* ctx) {
                s_notified = (ctx != nullptr) ? 20 : 2;
            }
        };
        void* vt0[20]{}; vt0[17] = reinterpret_cast<void*>(&FakeEntry::onNotify0);
        void* vt1[20]{}; vt1[17] = reinterpret_cast<void*>(&FakeEntry::onNotify1);

        alignas(16) std::uint8_t e0[0x30]{}, e1[0x30]{};
        writePtr(e0, 0x00, vt0);
        writeU32(e0, 0x0C, parseUint(argv[5]));
        writeU32(e0, 0x10, parseUint(argv[6]));
        writeI32(e0, 0x14, parseInt(argv[7]));

        writePtr(e1, 0x00, vt1);
        writeU32(e1, 0x0C, parseUint(argv[9]));
        writeU32(e1, 0x10, parseUint(argv[10]));
        writeI32(e1, 0x14, parseInt(argv[11]));

        RbTreeNodeView n0{}, n1{};
        n0.keyAt10 = 10u;
        n0.valueAt14 = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(e0));
        n1.keyAt10 = 20u;
        n1.valueAt14 = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(e1));
        n1.leftAt4 = &n0;

        alignas(16) std::uint8_t reg[0x30]{};
        writePtr(reg, 0x08 + 0x0C, &n1);

        std::uint32_t keys[2] = { 10u, 20u };
        int dummyCtx = 123;
        alignas(16) std::uint8_t self[0x550]{};
        writePtr(self, 0x264, &dummyCtx);
        writePtr(self, 0x538, reg);
        writePtr(self, 0x53C, keys);
        writePtr(self, 0x540, keys + count);

        FakeThingFactoryHash tf;
        alignas(16) std::uint8_t td0[0xA0]{}, td1[0xA0]{};
        alignas(16) std::uint8_t tmpl0[0x20]{}, tmpl1[0x20]{};
        writeI32(td0, 0x8C, parseInt(argv[8]));
        writePtr(tmpl0, 0x14, td0);
        writeI32(td1, 0x8C, parseInt(argv[12]));
        writePtr(tmpl1, 0x14, td1);
        tf.addTemplate(0, parseUint(argv[6]), tmpl0);
        if (parseUint(argv[10]) != parseUint(argv[6])) {
            tf.addTemplate(1, parseUint(argv[10]), tmpl1);
        }

        if (op == "sum_queued_power_88ad80") {
            const std::int32_t r = sumQueuedPowerDelta_88AD80(self, tf.factory);
            std::cout << "ret=" << r << "\n";
        } else if (op == "count_queued_tid_88ae10") {
            const std::int32_t r = countQueuedTemplate_88AE10(self, targetTid);
            std::cout << "ret=" << r << "\n";
        } else {
            notifyObjectConstructed_88AFC0(self, builderId, &targetTid);
            std::cout << "notified=" << s_notified << "\n";
        }
        return 0;
    }

    // 18. 0x0088BC10 & 35. 0x00897BB0
    // args: owner8 owner4
    if ((op == "init_econ_owner_88bc10" || op == "init_econ_default_897bb0") && argc == 4) {
        alignas(16) std::uint8_t self[0x60]{};
        std::memset(self, 0xCC, sizeof(self));
        void* o8 = reinterpret_cast<void*>(static_cast<std::uintptr_t>(parseUint(argv[2])));
        void* o4 = reinterpret_cast<void*>(static_cast<std::uintptr_t>(parseUint(argv[3])));
        void* r = (op == "init_econ_owner_88bc10")
            ? initEconomyStatsWithOwner_88BC10(self, o8, o4)
            : initEconomyStatsDefault_897BB0(self);
        std::cout << "ret_eq=" << (r == self ? 1 : 0)
                  << " vft=" << hex32(readU32(self, 0x00))
                  << " f04=" << hex32(readU32(self, 0x04))
                  << " f08=" << hex32(readU32(self, 0x08))
                  << " f0c=" << readU32(self, 0x0C)
                  << " self1c=" << (readU32(self, 0x1C) == reinterpret_cast<std::uintptr_t>(self + 0x1C) ? 1 : 0)
                  << " self20=" << (readU32(self, 0x20) == reinterpret_cast<std::uintptr_t>(self + 0x1C) ? 1 : 0)
                  << " f24=" << readU32(self, 0x24)
                  << " self48=" << (readU32(self, 0x48) == reinterpret_cast<std::uintptr_t>(self + 0x48) ? 1 : 0)
                  << " self4c=" << (readU32(self, 0x4C) == reinterpret_cast<std::uintptr_t>(self + 0x48) ? 1 : 0)
                  << " f58=" << readU32(self, 0x58) << "\n";
        return 0;
    }

    // 19. 0x0088BE50 findStatValueByKey_88BE50
    // args: useTarget8 queryKey k0 v0 k1 v1
    if (op == "find_stat_val_88be50" && argc == 8) {
        const int useTarget8 = parseInt(argv[2]);
        const std::uint32_t queryKey = parseUint(argv[3]);
        RbTreeNodeView n0{}, n1{};
        n0.keyAt10 = parseUint(argv[4]);
        n0.valueAt14 = parseUint(argv[5]);
        n1.keyAt10 = parseUint(argv[6]);
        n1.valueAt14 = parseUint(argv[7]);
        n1.leftAt4 = &n0;

        alignas(16) std::uint8_t target[0x60]{};
        alignas(16) std::uint8_t self[0x60]{};
        writePtr(target, 0x44 + 0x0C, &n1);
        writePtr(self, 0x44 + 0x0C, &n1);
        writePtr(self, 0x08, useTarget8 ? target : nullptr);

        const std::uint32_t r = findStatValueByKey_88BE50(self, queryKey);
        std::cout << "ret=" << r << "\n";
        return 0;
    }

    // 20. 0x0088BE90 pruneExpiredSamples_88BE90 & 36. 0x00897ED0 updateEconomyStatsSlidingWindow_897ED0
    // args: curFrame windowFrames count initSum f0 v0 f1 v1
    if ((op == "prune_samples_88be90" || op == "update_econ_window_897ed0") && argc == 10) {
        const std::uint32_t curFrame = parseUint(argv[2]);
        const std::uint32_t windowFrames = parseUint(argv[3]);
        const int count = parseInt(argv[4]);
        const float initSum = parseFloat(argv[5]);

        alignas(16) std::uint8_t gameLogic[0x80]{};
        writeU32(gameLogic, 0x58, curFrame);

        EconomySampleView block0[32]{};
        block0[0] = { parseUint(argv[6]), parseFloat(argv[7]) };
        block0[1] = { parseUint(argv[8]), parseFloat(argv[9]) };
        EconomySampleView* map0[2] = { block0, block0 };

        EconomySampleView block1[32]{};
        block1[0] = { parseUint(argv[6]), parseFloat(argv[7]) };
        block1[1] = { parseUint(argv[8]), parseFloat(argv[9]) };
        EconomySampleView* map1[2] = { block1, block1 };

        auto initWin = [&](std::uint8_t* w, EconomySampleView* blk, EconomySampleView** mp) {
            writePtr(w, 0x08, blk);
            writePtr(w, 0x0C, blk);
            writePtr(w, 0x10, reinterpret_cast<std::uint8_t*>(blk) + 0x100);
            writePtr(w, 0x14, mp);
            writePtr(w, 0x18, blk + count);
            writeF32(w, 0x2C, initSum);
            writeU32(w, 0x34, windowFrames);
        };

        if (op == "prune_samples_88be90") {
            alignas(16) std::uint8_t win[0x40]{};
            initWin(win, block0, map0);
            pruneExpiredSamples_88BE90(win, gameLogic);
            const int rem = static_cast<int>(
                (readU32(win, 0x18) - readU32(win, 0x08)) / sizeof(EconomySampleView));
            std::cout << "rem=" << rem << " sum=" << hex32(readU32(win, 0x2C)) << "\n";
        } else {
            alignas(16) std::uint8_t self[0x80]{};
            initWin(self + 0x08, block0, map0);
            initWin(self + 0x40, block1, map1);
            updateEconomyStatsSlidingWindow_897ED0(self, gameLogic);
            const int rem0 = static_cast<int>(
                (readU32(self + 0x08, 0x18) - readU32(self + 0x08, 0x08)) / sizeof(EconomySampleView));
            const int rem1 = static_cast<int>(
                (readU32(self + 0x40, 0x18) - readU32(self + 0x40, 0x08)) / sizeof(EconomySampleView));
            std::cout << "rem0=" << rem0 << " sum0=" << hex32(readU32(self + 0x08, 0x2C))
                      << " rem1=" << rem1 << " sum1=" << hex32(readU32(self + 0x40, 0x2C)) << "\n";
        }
        return 0;
    }

    // 21. 0x0088D120 groupContainsTemplateId_88D120 & 22. 0x0088D1A0 categoryMapContainsTemplateId_88D1A0
    // args: count targetTid t0 t1
    if ((op == "group_contains_88d120" || op == "catmap_contains_88d1a0") && argc == 6) {
        const int count = parseInt(argv[2]);
        const std::uint32_t targetTid = parseUint(argv[3]);

        alignas(16) std::uint8_t e0[0x20]{}, e1[0x20]{};
        writeU32(e0, 0x10, parseUint(argv[4]));
        writeU32(e1, 0x10, parseUint(argv[5]));

        RbTreeNodeView rn0{}, rn1{};
        rn0.keyAt10 = 1u;
        rn0.valueAt14 = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(e0));
        rn1.keyAt10 = 2u;
        rn1.valueAt14 = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(e1));
        rn1.leftAt4 = &rn0;

        alignas(16) std::uint8_t owner[0x30]{};
        writePtr(owner, 0x08 + 0x0C, &rn1);

        std::uint32_t keys[2] = { 1u, 2u };
        alignas(16) std::uint8_t grp[0x20]{};
        writePtr(grp, 0x0C, owner);
        writePtr(grp, 0x10, keys);
        writePtr(grp, 0x14, keys + count);

        if (op == "group_contains_88d120") {
            const bool r = groupContainsTemplateId_88D120(grp, targetTid);
            std::cout << "ret=" << (r ? 1 : 0) << "\n";
        } else {
            alignas(16) std::uint8_t self[0x30]{};
            auto* outerSent = reinterpret_cast<RbTreeNodeView*>(self + 0x08);
            outerSent->isSentinelAt2D = 1u;

            RbTreeNodeView outerNode{};
            auto* innerSent = reinterpret_cast<RbTreeNodeView*>(
                reinterpret_cast<std::uint8_t*>(&outerNode) + 0x18);
            writeU8(innerSent, 0x2D, 1u);

            RbTreeNodeView innerNode{};
            innerNode.rightAt0 = innerSent;
            innerNode.leftAt4 = innerSent;
            innerNode.parentAt8 = innerSent;
            innerNode.valueAt14 = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(grp));

            outerNode.rightAt0 = outerSent;
            outerNode.leftAt4 = outerSent;
            outerNode.parentAt8 = outerSent;
            outerNode.nestedLeftmostAt1C = (count > 0) ? &innerNode : innerSent;

            writePtr(self, 0x0C, (count > 0) ? &outerNode : outerSent);
            const bool r = categoryMapContainsTemplateId_88D1A0(self, targetTid);
            std::cout << "ret=" << (r ? 1 : 0) << "\n";
        }
        return 0;
    }

    // 23. 0x0088DE50 advanceToNextActive_88DE50 & 24. 0x0088DEC0 findFirstActiveForKey_88DEC0
    // args: queryKey k0 blk0 k1 blk1
    if ((op == "advance_next_active_88de50" || op == "find_first_active_88dec0") && argc == 7) {
        const std::uint32_t queryKey = parseUint(argv[2]);
        alignas(16) std::uint8_t self[0x40]{};
        auto* sent = reinterpret_cast<RbTreeNodeView*>(self + 0x08);
        sent->isSentinelAt2D = 1u;

        RbTreeNodeView n0{}, n1{};
        // n0 is root, n1 is right child of n0 (keys k0 <= k1)
        n0.keyAt10 = parseUint(argv[3]);
        n0.blockedFlagAt28 = parseUint(argv[4]);
        n0.leftAt4 = nullptr;
        n0.rightAt0 = &n1;
        n0.parentAt8 = sent;

        n1.keyAt10 = parseUint(argv[5]);
        n1.blockedFlagAt28 = parseUint(argv[6]);
        n1.leftAt4 = nullptr;
        n1.rightAt0 = nullptr;
        n1.parentAt8 = &n0;

        sent->parentAt8 = &n0;
        writePtr(self, 0x10, &n0);

        AIBuildableIteratorView iter{};
        if (op == "advance_next_active_88de50") {
            iter.nodeAt0 = &n0;
            iter.keyAt4 = queryKey;
            iter.ownerMapAt8 = self;
            advanceToNextActive_88DE50(&iter);
        } else {
            findFirstActiveForKey_88DEC0(self, &iter, queryKey);
        }
        const int which = (iter.nodeAt0 == &n0) ? 0 : (iter.nodeAt0 == &n1) ? 1 : (iter.nodeAt0 == sent) ? 2 : -1;
        std::cout << "node=" << which << " key=" << iter.keyAt4 << " hasOwner=" << (iter.ownerMapAt8 != nullptr ? 1 : 0) << "\n";
        return 0;
    }

    // 26. 0x00890400, 37. 0x0089A6D0, 38. 0x0089A740
    // args: objId inHash dead471 ox oy oz fx fy fz
    if ((op == "get_base_pos_890400" || op == "resolve_struct_obj_89a6d0" || op == "get_struct_pos_89a740") && argc == 11) {
        FakeGameLogicHash gl;
        const std::uint32_t objId = parseUint(argv[2]);
        const int inHash = parseInt(argv[3]);
        const int dead471 = parseInt(argv[4]);

        alignas(16) std::uint8_t obj[0x480]{};
        writeF32(obj, 0x38, parseFloat(argv[5]));
        writeF32(obj, 0x3C, parseFloat(argv[6]));
        writeF32(obj, 0x40, parseFloat(argv[7]));
        writeU8(obj, 0x471, static_cast<std::uint8_t>(dead471));
        if (inHash && objId != 0u) {
            gl.addObject(0, objId, obj);
        }

        if (op == "get_base_pos_890400") {
            AIBaseView base{};
            base.centerObjectIdAt0C = objId;
            base.fallbackPositionAt10 = { parseFloat(argv[8]), parseFloat(argv[9]), parseFloat(argv[10]) };
            const Coord3DView* p = getBasePosition_890400(&base, gl.gameLogic);
            std::cout << "x=" << floatBitsHex(p->x) << " y=" << floatBitsHex(p->y) << " z=" << floatBitsHex(p->z) << "\n";
        } else {
            alignas(16) std::uint8_t entry[0x30]{};
            writeF32(entry, 0x10, parseFloat(argv[8]));
            writeF32(entry, 0x14, parseFloat(argv[9]));
            writeF32(entry, 0x18, parseFloat(argv[10]));
            writeU32(entry, 0x20, objId);
            if (op == "resolve_struct_obj_89a6d0") {
                void* r = resolveStructureObject_89A6D0(entry, gl.gameLogic);
                std::cout << "found=" << (r == obj ? 1 : 0) << "\n";
            } else {
                const Coord3DView* p = getStructureOrFallbackPosition_89A740(entry, gl.gameLogic);
                std::cout << "x=" << floatBitsHex(p->x) << " y=" << floatBitsHex(p->y) << " z=" << floatBitsHex(p->z) << "\n";
            }
        }
        return 0;
    }

    // 27. 0x00890600 & 28. 0x008908C0
    // args: reqMask forbMask prefMask dislMask sortMode id0 fl0 cnt0 m0 x0 id1 fl1 cnt1 m1 x1 refX
    if ((op == "is_candidate_better_890600" || op == "find_best_base_8908c0") && argc == 18) {
        FakeGameLogicHash gl;
        std::uint32_t dummyStructs[8]{};

        AIBaseQueryCriteriaConfigView cfg{};
        cfg.requiredMaskAt0 = parseUint(argv[2]);
        cfg.forbiddenMaskAt4 = parseUint(argv[3]);
        cfg.preferredMaskAt8 = parseUint(argv[4]);
        cfg.dislikedMaskAtC = parseUint(argv[5]);
        cfg.sortModeAt10 = parseUint(argv[6]);

        AIBaseView b0{}, b1{};
        b0.baseIdAt4 = parseInt(argv[7]);
        b0.flagsAt2C = parseUint(argv[8]);
        b0.structuresBeginAt30 = dummyStructs;
        b0.structuresEndAt34 = dummyStructs + parseUint(argv[9]);
        b0.metricAt40 = parseUint(argv[10]);
        b0.fallbackPositionAt10 = { parseFloat(argv[11]), 0.0f, 0.0f };

        b1.baseIdAt4 = parseInt(argv[12]);
        b1.flagsAt2C = parseUint(argv[13]);
        b1.structuresBeginAt30 = dummyStructs;
        b1.structuresEndAt34 = dummyStructs + parseUint(argv[14]);
        b1.metricAt40 = parseUint(argv[15]);
        b1.fallbackPositionAt10 = { parseFloat(argv[16]), 0.0f, 0.0f };

        const Coord3DView refPos{ parseFloat(argv[17]), 0.0f, 0.0f };

        if (op == "is_candidate_better_890600") {
            AIBaseQueryCriteriaView crit{
                cfg.requiredMaskAt0, cfg.forbiddenMaskAt4,
                cfg.preferredMaskAt8, cfg.dislikedMaskAtC,
                cfg.sortModeAt10, &refPos
            };
            const bool r = isCandidateBetterThanBest_890600(&crit, &b0, &b1, gl.gameLogic);
            std::cout << "ret=" << (r ? 1 : 0) << "\n";
        } else {
            AIBaseView* arr[2] = { &b0, &b1 };
            alignas(16) std::uint8_t self[0x34]{};
            writePtr(self, 0x28, arr);
            writePtr(self, 0x2C, arr + 2);
            const AIBaseQueryCriteriaConfigView* cfgPtr =
                (cfg.requiredMaskAt0 == 0xFFFFFFFFu) ? nullptr : &cfg;
            AIBaseView* best = findBestMatchingBase_8908C0(self, cfgPtr, &refPos, gl.gameLogic);
            const int idx = (best == &b0) ? 0 : (best == &b1) ? 1 : -1;
            std::cout << "best=" << idx << "\n";
        }
        return 0;
    }

    // 29. 0x008925B0 claimFirstMatchingBuilderSlot_8925B0
    // args: s0_claim s0_oid s0_a s0_b s0_exp s0_ovr s1_claim s1_oid s1_a s1_b s1_exp s1_ovr
    if (op == "claim_builder_slot_8925b0" && argc == 14) {
        FakeGameLogicHash gl;
        alignas(16) std::uint8_t prod0[0xC0]{}, mod0[0x200]{}, obj0[0x420]{};
        alignas(16) std::uint8_t prod1[0xC0]{}, mod1[0x200]{}, obj1[0x420]{};

        writeU32(prod0, 0xAC, parseUint(argv[6]));
        writeU32(mod0, 0x1F8, parseUint(argv[7]));
        writePtr(obj0, 0x414, mod0);
        writePtr(obj0, 0x418, prod0);

        writeU32(prod1, 0xAC, parseUint(argv[12]));
        writeU32(mod1, 0x1F8, parseUint(argv[13]));
        writePtr(obj1, 0x414, mod1);
        writePtr(obj1, 0x418, prod1);

        const std::uint32_t oid0 = parseUint(argv[3]);
        const std::uint32_t oid1 = parseUint(argv[9]);
        if (oid0 != 0u) gl.addObject(0, oid0, obj0);
        if (oid1 != 0u) gl.addObject(1, oid1, obj1);

        alignas(16) std::uint8_t slots[2 * 0x0C]{};
        writeU32(slots, 0x00, parseUint(argv[2]));
        writeU32(slots, 0x04, oid0);
        writeU8(slots, 0x0A, static_cast<std::uint8_t>(parseUint(argv[4])));
        writeU8(slots, 0x0B, static_cast<std::uint8_t>(parseUint(argv[5])));

        writeU32(slots, 0x0C + 0x00, parseUint(argv[8]));
        writeU32(slots, 0x0C + 0x04, oid1);
        writeU8(slots, 0x0C + 0x0A, static_cast<std::uint8_t>(parseUint(argv[10])));
        writeU8(slots, 0x0C + 0x0B, static_cast<std::uint8_t>(parseUint(argv[11])));

        alignas(16) std::uint8_t self[0x20]{};
        writePtr(self, 0x0C, slots);
        writePtr(self, 0x10, slots + 2 * 0x0C);

        const std::uint32_t r = claimFirstMatchingBuilderSlot_8925B0(self, gl.gameLogic);
        std::cout << "ret=" << r
                  << " b0=" << static_cast<unsigned>(readU8(slots, 0x0B))
                  << " b1=" << static_cast<unsigned>(readU8(slots, 0x0C + 0x0B)) << "\n";
        return 0;
    }

    // 30. 0x00892670 hasEnoughPowerForTemplate_892670
    // args: reqPwr produced consumed empty20 queuedPwr
    if (op == "has_enough_power_892670" && argc == 7) {
        FakeThingFactoryHash tf;
        alignas(16) std::uint8_t tdReq[0xA0]{}, tmplReq[0x20]{};
        alignas(16) std::uint8_t tdQ[0xA0]{}, tmplQ[0x20]{};
        writeI32(tdReq, 0x8C, parseInt(argv[2]));
        writePtr(tmplReq, 0x14, tdReq);
        writeI32(tdQ, 0x8C, parseInt(argv[6]));
        writePtr(tmplQ, 0x14, tdQ);
        tf.addTemplate(0, 1u, tmplReq);
        tf.addTemplate(1, 2u, tmplQ);

        alignas(16) std::uint8_t powerState[0x30]{};
        writeI32(powerState, 0x04, parseInt(argv[3]));
        writeI32(powerState, 0x08, parseInt(argv[4]));
        writePtr(powerState, 0x20, parseInt(argv[5]) ? (powerState + 0x20) : nullptr);

        alignas(16) std::uint8_t qEntry[0x20]{};
        writeU32(qEntry, 0x10, 2u);
        RbTreeNodeView qNode{};
        qNode.keyAt10 = 10u;
        qNode.valueAt14 = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(qEntry));
        alignas(16) std::uint8_t qReg[0x30]{};
        writePtr(qReg, 0x08 + 0x0C, &qNode);
        std::uint32_t qKeys[1] = { 10u };

        alignas(16) std::uint8_t aiBuilderOwner[0x650]{};
        writePtr(aiBuilderOwner, 0x104 + 0x538, qReg);
        writePtr(aiBuilderOwner, 0x104 + 0x53C, qKeys);
        writePtr(aiBuilderOwner, 0x104 + 0x540, qKeys + (parseInt(argv[6]) != 0 ? 1 : 0));

        alignas(16) std::uint8_t owner14[0xA0]{};
        writeI32(owner14, 0x24, 1);
        writePtr(owner14, 0x74, powerState);
        writePtr(owner14, 0x90, aiBuilderOwner);

        alignas(16) std::uint8_t powerOvrMgr[0x40]{};
        alignas(16) std::uint8_t self[0x20]{};
        writePtr(self, 0x14, owner14);

        alignas(16) std::uint8_t buildReq[0x20]{};
        writeU32(buildReq, 0x10, 1u);

        const bool r = hasEnoughPowerForTemplate_892670(self, buildReq, tf.factory, powerOvrMgr);
        std::cout << "ret=" << (r ? 1 : 0) << "\n";
        return 0;
    }

    // 31. 0x00893520 shouldEstablishNewBase_893520
    // args: mode198 dx198 rad40 flag244 vecCnt25c f44 f264 f260 baseX minDist3c f38 f23c
    if (op == "should_establish_base_893520" && argc == 14) {
        FakeGameLogicHash gl;
        alignas(16) std::uint8_t cfg[0xA0]{};
        writeF32(cfg, 0x38, parseFloat(argv[12]));
        writeF32(cfg, 0x3C, parseFloat(argv[11]));
        writeF32(cfg, 0x40, parseFloat(argv[4]));
        writeF32(cfg, 0x44, parseFloat(argv[7]));
        writePtr(cfg, 0x94, static_cast<void*>(nullptr));

        AIBaseView b0{};
        b0.baseIdAt4 = 1;
        b0.flagsAt2C = 1u;
        b0.fallbackPositionAt10 = { parseFloat(argv[10]), 0.0f, 0.0f };
        AIBaseView* bases[1] = { &b0 };

        alignas(16) std::uint8_t aiPlayer[0x490]{};
        writePtr(aiPlayer, 0x454 + 0x28, bases);
        writePtr(aiPlayer, 0x454 + 0x2C, bases + (parseFloat(argv[10]) >= 0.0f ? 1 : 0));

        alignas(16) std::uint8_t owner0[0xA0]{};
        writePtr(owner0, 0x90, aiPlayer);

        std::uint32_t dummyVec[4]{};
        alignas(16) std::uint8_t self[0x270]{};
        writePtr(self, 0x00, owner0);
        writePtr(self, 0x18, cfg);
        writeI32(self, 0x198, parseInt(argv[2]));
        writeF32(self, 0x1A4, parseFloat(argv[3]));
        writeF32(self, 0x1A8, 0.0f);
        writeF32(self, 0x1AC, 0.0f);
        writePtr(self, 0x1CC, dummyVec);
        writePtr(self, 0x1D0, dummyVec + 2);
        writeF32(self, 0x23C, parseFloat(argv[13]));
        writeU8(self, 0x244, static_cast<std::uint8_t>(parseUint(argv[5])));
        writeF32(self, 0x248, 0.0f);
        writeF32(self, 0x24C, 0.0f);
        writeF32(self, 0x250, 0.0f);
        writeU32(self, 0x25C, parseUint(argv[6]));
        writeF32(self, 0x260, parseFloat(argv[9]));
        writeF32(self, 0x264, parseFloat(argv[8]));

        const bool r = shouldEstablishNewBase_893520(self, gl.gameLogic);
        std::cout << "ret=" << (r ? 1 : 0) << "\n";
        return 0;
    }

    // 32. 0x00893880, 33. 0x00893920, 34. 0x00893A00
    // args: queryKey nodeKey objId auxId inHash dead471
    if ((op == "find_active_slot_893880" || op == "get_slot_aux_893920" || op == "has_alive_tracked_893a00") && argc == 8) {
        FakeGameLogicHash gl;
        const std::int32_t queryKey = parseInt(argv[2]);
        const std::int32_t nodeKey = parseInt(argv[3]);
        const std::uint32_t objId = parseUint(argv[4]);
        const std::uint32_t auxId = parseUint(argv[5]);
        const int inHash = parseInt(argv[6]);
        const int dead471 = parseInt(argv[7]);

        alignas(16) std::uint8_t obj[0x480]{};
        writeU8(obj, 0x471, static_cast<std::uint8_t>(dead471));
        if (inHash && objId != 0u) {
            gl.addObject(0, objId, obj);
        }

        RbTreeNodeView node{};
        node.keyAt10 = static_cast<std::uint32_t>(nodeKey);
        node.valueAt14 = objId;
        node.auxValueAt18 = auxId;

        alignas(16) std::uint8_t self[0xB0]{};
        writePtr(self, 0x98 + 0x0C, &node);
        writePtr(self, 0x7C + 0x0C, &node);

        if (op == "find_active_slot_893880") {
            void* r = findActiveObjectBySlotKey_893880(self, queryKey, gl.gameLogic);
            std::cout << "found=" << (r == obj ? 1 : 0) << "\n";
        } else if (op == "get_slot_aux_893920") {
            const std::uint32_t r = getSlotAuxiliaryIdIfObjectAlive_893920(self, queryKey, gl.gameLogic);
            std::cout << "ret=" << r << "\n";
        } else {
            alignas(16) std::uint8_t cand[0xE8]{};
            writeI32(cand, 0xDC, queryKey);
            const bool r = hasAliveTrackedObjectForCandidate_893A00(self, cand, gl.gameLogic);
            std::cout << "ret=" << (r ? 1 : 0) << "\n";
        }
        return 0;
    }

    // 39. 0x0089AC90 initBuildableStructureTracker_89AC90
    // args: frame58 scale1c4 fps
    if (op == "init_struct_tracker_89ac90" && argc == 5) {
        alignas(16) std::uint8_t gl[0x1D0]{};
        writeU32(gl, 0x58, parseUint(argv[2]));
        writeF32(gl, 0x1C4, parseFloat(argv[3]));
        const float fps = parseFloat(argv[4]);

        alignas(16) std::uint8_t self[0x50]{};
        std::memset(self, 0xCC, sizeof(self));
        void* r = initBuildableStructureTracker_89AC90(self, gl, &fps);
        std::cout << "ret_eq=" << (r == self ? 1 : 0)
                  << " vft=" << hex32(readU32(self, 0x00))
                  << " f08=" << hex32(readU32(self, 0x08))
                  << " f0c=" << readU32(self, 0x0C)
                  << " f14=" << hex32(readU32(self, 0x14))
                  << " f18=" << hex32(readU32(self, 0x18))
                  << " f28=" << hex32(readU32(self, 0x28))
                  << " f44=" << readU32(self, 0x44) << "\n";
        return 0;
    }

    // 40. 0x0089AF50 initBuildHeuristicBase_89AF50
    if (op == "init_heur_base_89af50" && argc == 2) {
        alignas(16) std::uint8_t self[0x30]{};
        std::memset(self, 0xCC, sizeof(self));
        void* r = initBuildHeuristicBase_89AF50(self);
        std::cout << "ret_eq=" << (r == self ? 1 : 0)
                  << " vft=" << hex32(readU32(self, 0x00))
                  << " f04=" << readU32(self, 0x04)
                  << " f08=" << readU32(self, 0x08)
                  << " f0c=" << readU32(self, 0x0C)
                  << " f14=" << readU32(self, 0x14)
                  << " f18=" << readU32(self, 0x18)
                  << " f1c=" << readU32(self, 0x1C)
                  << " f24=" << static_cast<unsigned>(readU8(self, 0x24)) << "\n";
        return 0;
    }

    // 41. 0x0089D070 & 42. 0x0089D0D0
    // args: count queryId qx qy qz e0_id e0_74 e0_inHash e0_x e1_id e1_74 e1_inHash e1_x
    if ((op == "has_active_entry_id_89d070" || op == "has_active_entry_pos_89d0d0") && argc == 15) {
        FakeGameLogicHash gl;
        const std::uint32_t count = parseUint(argv[2]);
        const std::uint32_t queryId = parseUint(argv[3]);
        const float qx = parseFloat(argv[4]);
        const float qy = parseFloat(argv[5]);
        const float qz = parseFloat(argv[6]);

        alignas(16) std::uint8_t obj0[0x480]{}, obj1[0x480]{};
        writeF32(obj0, 0x38, parseFloat(argv[10]));
        writeF32(obj0, 0x3C, qy);
        writeF32(obj0, 0x40, qz);
        writeF32(obj1, 0x38, parseFloat(argv[14]));
        writeF32(obj1, 0x3C, qy);
        writeF32(obj1, 0x40, qz);

        const std::uint32_t id0 = parseUint(argv[7]);
        const std::uint32_t id1 = parseUint(argv[11]);
        if (parseInt(argv[9]) && id0 != 0u) gl.addObject(0, id0, obj0);
        if (parseInt(argv[13]) && id1 != 0u) gl.addObject(1, id1, obj1);

        alignas(16) std::uint8_t entries[2 * 0x2CC]{};
        writeF32(entries, 0x10, parseFloat(argv[10]));
        writeF32(entries, 0x14, qy);
        writeF32(entries, 0x18, qz);
        writeU32(entries, 0x20, id0);
        writeU8(entries, 0x74, static_cast<std::uint8_t>(parseUint(argv[8])));

        writeF32(entries, 0x2CC + 0x10, parseFloat(argv[14]));
        writeF32(entries, 0x2CC + 0x14, qy);
        writeF32(entries, 0x2CC + 0x18, qz);
        writeU32(entries, 0x2CC + 0x20, id1);
        writeU8(entries, 0x2CC + 0x74, static_cast<std::uint8_t>(parseUint(argv[12])));

        alignas(16) std::uint8_t reg[0x50]{};
        writePtr(reg, 0x38, entries);
        writeU32(reg, 0x3C, count);

        alignas(16) std::uint8_t self[0x700]{};
        writePtr(self, 0x6F4, reg);

        const bool r = (op == "has_active_entry_id_89d070")
            ? hasActiveBuildableEntryForObjectId_89D070(self, queryId, gl.gameLogic)
            : hasActiveBuildableEntryAtPosition_89D0D0(self, qx, qy, qz, gl.gameLogic);
        std::cout << "ret=" << (r ? 1 : 0) << "\n";
        return 0;
    }

    // 43. 0x0089DD90 eraseCandidateRange68_89DD90
    // args: total beginIdx endIdx
    if (op == "erase_cand_range_89dd90" && argc == 5) {
        const int total = parseInt(argv[2]);
        const int beginIdx = parseInt(argv[3]);
        const int endIdx = parseInt(argv[4]);
        alignas(16) std::uint8_t buf[6 * 0x44]{};
        for (int i = 0; i < total; ++i) {
            writeU32(buf, i * 0x44, static_cast<std::uint32_t>(10 + i));
            writeU32(buf, i * 0x44 + 0x40, static_cast<std::uint32_t>(100 + i));
        }
        alignas(16) std::uint8_t self[0x10]{};
        writePtr(self, 0x00, buf);
        writePtr(self, 0x04, buf + total * 0x44);

        void* r = eraseCandidateRange68_89DD90(
            self, buf + beginIdx * 0x44, buf + endIdx * 0x44);
        const int newTotal = static_cast<int>(
            (readU32(self, 0x04) - reinterpret_cast<std::uintptr_t>(buf)) / 0x44);
        const int retIdx = static_cast<int>(
            (static_cast<std::uint8_t*>(r) - buf) / 0x44);
        std::cout << "retIdx=" << retIdx << " newTotal=" << newTotal
                  << " v0=" << readU32(buf, 0)
                  << " v1=" << readU32(buf, 0x44)
                  << " v2=" << readU32(buf, 2 * 0x44) << "\n";
        return 0;
    }

    // 44. 0x0089E700, 45. 0x0089E750, 46. 0x0089F140, 47. 0x0089F1D0
    // args: sortMode id0 id1 id2 id3
    if ((op == "sort_bounded_89e700" || op == "sort_unguarded_89e750" || op == "median3_89f140" || op == "partition_89f1d0") && argc == 7) {
        FakeGameLogicHash gl;
        AIBaseQueryCriteriaView crit{};
        crit.sortModeAt10 = parseUint(argv[2]);

        AIBaseView b[4]{};
        AIBaseView* ptrs[4]{};
        for (int i = 0; i < 4; ++i) {
            b[i].baseIdAt4 = parseInt(argv[3 + i]);
            ptrs[i] = &b[i];
        }

        if (op == "sort_bounded_89e700") {
            insertionSortBasesBounded_89E700(ptrs, ptrs + 4, crit, gl.gameLogic);
            std::cout << "o=" << ptrs[0]->baseIdAt4 << "," << ptrs[1]->baseIdAt4
                      << "," << ptrs[2]->baseIdAt4 << "," << ptrs[3]->baseIdAt4 << "\n";
        } else if (op == "sort_unguarded_89e750") {
            // Ensure sentinel at ptrs[0] is strictly best so unguarded loop terminates in bounds
            b[0].baseIdAt4 = (crit.sortModeAt10 == 0u) ? -999 : 999;
            insertionSortBasesUnguarded_89E750(ptrs + 1, ptrs + 4, crit, gl.gameLogic);
            std::cout << "o=" << ptrs[0]->baseIdAt4 << "," << ptrs[1]->baseIdAt4
                      << "," << ptrs[2]->baseIdAt4 << "," << ptrs[3]->baseIdAt4 << "\n";
        } else if (op == "median3_89f140") {
            AIBaseView** m = selectMedianOfThreeBases_89F140(&ptrs[0], &ptrs[1], &ptrs[2], crit, gl.gameLogic);
            const int idx = static_cast<int>(m - ptrs);
            std::cout << "idx=" << idx << " id=" << (*m)->baseIdAt4 << "\n";
        } else {
            AIBaseView* pivot = ptrs[1];
            AIBaseView** cut = partitionBasesByPivot_89F1D0(ptrs, ptrs + 4, &pivot, crit, gl.gameLogic);
            const int idx = static_cast<int>(cut - ptrs);
            std::cout << "cut=" << idx << " o=" << ptrs[0]->baseIdAt4 << "," << ptrs[1]->baseIdAt4
                      << "," << ptrs[2]->baseIdAt4 << "," << ptrs[3]->baseIdAt4 << "\n";
        }
        return 0;
    }

    // 48. 0x0089F480 initBuildHeuristicStore_89F480
    if (op == "init_heur_store_89f480" && argc == 2) {
        alignas(16) std::uint8_t self[0x260]{};
        std::memset(self, 0xCC, sizeof(self));
        void* r = initBuildHeuristicStore_89F480(self);
        std::cout << "ret_eq=" << (r == self ? 1 : 0)
                  << " v0=" << hex32(readU32(self, 0x000))
                  << " v4=" << hex32(readU32(self, 0x004))
                  << " s0c=" << (readU32(self, 0x00C) == reinterpret_cast<std::uintptr_t>(self + 0x00C) ? 1 : 0)
                  << " vc8=" << hex32(readU32(self, 0x0C8))
                  << " ve8=" << hex32(readU32(self, 0x0E8))
                  << " v108=" << hex32(readU32(self, 0x108))
                  << " f114=" << readU32(self, 0x114)
                  << " v11c=" << hex32(readU32(self, 0x11C))
                  << " v23c=" << hex32(readU32(self, 0x23C))
                  << " s244=" << (readU32(self, 0x244) == reinterpret_cast<std::uintptr_t>(self + 0x244) ? 1 : 0)
                  << " f254=" << readU32(self, 0x254) << "\n";
        return 0;
    }

    // 49. 0x0089FE10 pruneAssignedStructures_89FE10
    // args: actCount a0 a1 a2 b0_s0 b0_s1
    if (op == "prune_assigned_89fe10" && argc == 8) {
        const int actCount = parseInt(argv[2]);
        std::uint32_t act[4] = { parseUint(argv[3]), parseUint(argv[4]), parseUint(argv[5]), 0u };
        std::uint32_t bs[2] = { parseUint(argv[6]), parseUint(argv[7]) };

        AIBaseView base{};
        base.structuresBeginAt30 = bs;
        base.structuresEndAt34 = bs + 2;
        AIBaseView* bases[1] = { &base };

        alignas(16) std::uint8_t self[0x70]{};
        writePtr(self, 0x28, bases);
        writePtr(self, 0x2C, bases + 1);
        writePtr(self, 0x5C, act);
        writePtr(self, 0x60, act + actCount);

        pruneAssignedStructures_89FE10(self);
        const int rem = static_cast<int>(
            (readU32(self, 0x60) - reinterpret_cast<std::uintptr_t>(act)) / 4);
        std::cout << "rem=" << rem << " a0=" << act[0] << " a1=" << act[1] << " a2=" << act[2] << "\n";
        return 0;
    }

    // 50. 0x0089FEB0 removeMatchingOrProducedTemplate_89FEB0
    // args: count targetTid t0 t1 eqChildOfT1
    if (op == "remove_matching_tmpl_89feb0" && argc == 7) {
        FakeThingFactoryHash tf;
        const int count = parseInt(argv[2]);
        const std::uint32_t targetTid = parseUint(argv[3]);
        std::uint32_t vec[3] = { parseUint(argv[4]), parseUint(argv[5]), 99u };
        const std::uint32_t eqChild = parseUint(argv[6]);

        alignas(16) std::uint8_t tdTarget[0x1D0]{}, tmplTarget[0x20]{};
        alignas(16) std::uint8_t td0[0x1D0]{}, tmpl0[0x20]{};
        alignas(16) std::uint8_t td1[0x1D0]{}, tmpl1[0x20]{};

        writeU32(tmplTarget, 0x08, targetTid);
        writePtr(tmplTarget, 0x14, tdTarget);

        writeU32(tmpl0, 0x08, vec[0]);
        writePtr(tmpl0, 0x14, td0);

        std::uint32_t childArr[1] = { eqChild };
        writeU32(td1, 0x1BC, eqChild != 0u ? 1u : 0u);
        writePtr(td1, 0x1C0, childArr);
        writeU32(tmpl1, 0x08, vec[1]);
        writePtr(tmpl1, 0x14, td1);

        tf.addTemplate(0, targetTid, tmplTarget);
        if (vec[0] != targetTid) tf.addTemplate(1, vec[0], tmpl0);
        if (vec[1] != targetTid && vec[1] != vec[0]) tf.addTemplate(2, vec[1], tmpl1);

        alignas(16) std::uint8_t self[0x30]{};
        writePtr(self, 0x24, vec);
        writePtr(self, 0x28, vec + count);

        removeMatchingOrProducedTemplate_89FEB0(self, targetTid, tf.factory);
        const int rem = static_cast<int>(
            (readU32(self, 0x28) - reinterpret_cast<std::uintptr_t>(vec)) / 4);
        std::cout << "rem=" << rem << " v0=" << vec[0] << " v1=" << vec[1] << "\n";
        return 0;
    }

    // 51. 0x008AAB70 getEffectivePriority_8AAB70
    // args: base08 ovr24
    if (op == "get_eff_prio_8aab70" && argc == 4) {
        alignas(16) std::uint8_t self[0x30]{};
        writeF32(self, 0x08, parseFloat(argv[2]));
        writeF32(self, 0x24, parseFloat(argv[3]));
        const float r = getEffectivePriority_8AAB70(self);
        std::cout << "ret=" << floatBitsHex(r) << "\n";
        return 0;
    }

    // 52. 0x008AABB0 & 53. 0x008AABE0
    // args: val cost14 cost15
    if ((op == "get_struct_vpc_8aabb0" || op == "get_unit_vpc_8aabe0") && argc == 5) {
        static float s_val = 0.0f, s_c14 = 0.0f, s_c15 = 0.0f;
        s_val = parseFloat(argv[2]);
        s_c14 = parseFloat(argv[3]);
        s_c15 = parseFloat(argv[4]);
        struct FakeBuildable {
            static float __fastcall fn13(void*, void*, void*) { return s_val; }
            static float __fastcall fn14(void*, void*, void*) { return s_c14; }
            static float __fastcall fn15(void*, void*, void*) { return s_c15; }
        };
        void* vtbl[16]{};
        vtbl[13] = reinterpret_cast<void*>(&FakeBuildable::fn13);
        vtbl[14] = reinterpret_cast<void*>(&FakeBuildable::fn14);
        vtbl[15] = reinterpret_cast<void*>(&FakeBuildable::fn15);
        void* self = vtbl;
        const float r = (op == "get_struct_vpc_8aabb0")
            ? getStructureValuePerCost_8AABB0(&self, nullptr)
            : getUnitValuePerCost_8AABE0(&self, nullptr);
        std::cout << "ret=" << floatBitsHex(r) << "\n";
        return 0;
    }

    // 54. 0x008AAC20 getScaledDelayFrames_8AAC20
    // args: hasGL scale1c4 fps
    if (op == "get_scaled_delay_8aac20" && argc == 5) {
        const int hasGL = parseInt(argv[2]);
        alignas(16) std::uint8_t gl[0x1D0]{};
        writeF32(gl, 0x1C4, parseFloat(argv[3]));
        const float fps = parseFloat(argv[4]);
        int dummySelf = 0;
        const float r = getScaledDelayFrames_8AAC20(&dummySelf, 0u, hasGL ? gl : nullptr, &fps);
        std::cout << "ret=" << floatBitsHex(r) << "\n";
        return 0;
    }

    // 55. 0x008AEEB0 & 56. 0x008AEF30
    // args: objId actCnt a0 a1 cmpCnt c0 c1
    if ((op == "move_to_under_const_8aeeb0" || op == "move_to_completed_8aef30") && argc == 9) {
        const std::uint32_t objId = parseUint(argv[2]);
        const int actCnt = parseInt(argv[3]);
        const int cmpCnt = parseInt(argv[6]);
        std::uint32_t act[4] = { parseUint(argv[4]), parseUint(argv[5]), 0u, 0u };
        std::uint32_t cmp[4] = { parseUint(argv[7]), parseUint(argv[8]), 0u, 0u };

        alignas(16) std::uint8_t self[0x80]{};
        writePtr(self, 0x5C, act);
        writePtr(self, 0x60, act + actCnt);
        writePtr(self, 0x64, act + 4);
        writePtr(self, 0x6C, cmp);
        writePtr(self, 0x70, cmp + cmpCnt);
        writePtr(self, 0x74, cmp + 4);

        if (op == "move_to_under_const_8aeeb0") {
            moveStructureToUnderConstruction_8AEEB0(self, objId);
        } else {
            moveStructureToCompleted_8AEF30(self, objId);
        }
        const int newAct = static_cast<int>(
            (readU32(self, 0x60) - reinterpret_cast<std::uintptr_t>(act)) / 4);
        const int newCmp = static_cast<int>(
            (readU32(self, 0x70) - reinterpret_cast<std::uintptr_t>(cmp)) / 4);
        std::cout << "actCnt=" << newAct << " a0=" << act[0] << " a1=" << act[1] << " a2=" << act[2]
                  << " cmpCnt=" << newCmp << " c0=" << cmp[0] << " c1=" << cmp[1] << " c2=" << cmp[2] << "\n";
        return 0;
    }

    std::cerr << "invalid arguments for op: " << op << " (argc=" << argc << ")\n";
    return 2;
}
