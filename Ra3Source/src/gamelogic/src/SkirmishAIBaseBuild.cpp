#include "ra3/gamelogic/SkirmishAIBaseBuild.h"

#include <cmath>
#include <cstring>
#if defined(_MSC_VER) && defined(_M_IX86)
#include <xmmintrin.h>
#endif

namespace ra3::gamelogic::skirmishai_base_build {

namespace {

inline std::uint32_t readU32(const void* base, std::ptrdiff_t offset) noexcept {
    std::uint32_t val = 0;
    std::memcpy(&val, static_cast<const std::uint8_t*>(base) + offset, sizeof(val));
    return val;
}

inline std::int32_t readI32(const void* base, std::ptrdiff_t offset) noexcept {
    std::int32_t val = 0;
    std::memcpy(&val, static_cast<const std::uint8_t*>(base) + offset, sizeof(val));
    return val;
}

inline std::uint8_t readU8(const void* base, std::ptrdiff_t offset) noexcept {
    return *(static_cast<const std::uint8_t*>(base) + offset);
}

inline float readF32(const void* base, std::ptrdiff_t offset) noexcept {
    float val = 0.0f;
    std::memcpy(&val, static_cast<const std::uint8_t*>(base) + offset, sizeof(val));
    return val;
}

template <typename T>
inline T* readPtr(const void* base, std::ptrdiff_t offset) noexcept {
    T* ptr = nullptr;
    std::memcpy(&ptr, static_cast<const std::uint8_t*>(base) + offset, sizeof(ptr));
    return ptr;
}

inline void writeU32(void* base, std::ptrdiff_t offset, std::uint32_t val) noexcept {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &val, sizeof(val));
}

inline void writeU8(void* base, std::ptrdiff_t offset, std::uint8_t val) noexcept {
    *(static_cast<std::uint8_t*>(base) + offset) = val;
}

inline void writeF32(void* base, std::ptrdiff_t offset, float val) noexcept {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &val, sizeof(val));
}

template <typename T>
inline void writePtr(void* base, std::ptrdiff_t offset, T* ptr) noexcept {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &ptr, sizeof(ptr));
}

inline const void* resolveGameLogic(const void* overridePtr) noexcept {
    if (overridePtr != nullptr) {
        return overridePtr;
    }
    return *reinterpret_cast<const void* const*>(kTheGameLogicSlotAddr);
}

inline const void* resolveThingFactory(const void* overridePtr) noexcept {
    if (overridePtr != nullptr) {
        return overridePtr;
    }
    return *reinterpret_cast<const void* const*>(kTheThingFactorySlotAddr);
}

inline void* resolveTerrainLogic(void* overridePtr) noexcept {
    if (overridePtr != nullptr) {
        return overridePtr;
    }
    return *reinterpret_cast<void* const*>(kTheTerrainLogicSlotAddr);
}

inline const void* resolvePowerOverrideMgr(const void* overridePtr) noexcept {
    if (overridePtr != nullptr) {
        return overridePtr;
    }
    return *reinterpret_cast<const void* const*>(kThePowerOverrideMgrSlotAddr);
}

inline float resolveFramesPerSecond(const float* overridePtr) noexcept {
    if (overridePtr != nullptr) {
        return *overridePtr;
    }
    return *reinterpret_cast<const float*>(kFramesPerSecondAddr);
}

// Helper matching 0x0050B5F0 (hash bucket lookup at tableBase: +4 = bucketsPtr, +8 = bucketCount)
void* findInHashTable_50B5F0(const void* tableBase, std::uint32_t key) noexcept {
    if (tableBase == nullptr) {
        return nullptr;
    }
    const std::uint32_t bucketCount = readU32(tableBase, 0x08);
    if (bucketCount == 0u) {
        return nullptr;
    }
    const auto* buckets = readPtr<const void* const>(tableBase, 0x04);
    if (buckets == nullptr) {
        return nullptr;
    }
    const std::uint32_t bucketIdx = key % bucketCount;
    const void* node = buckets[bucketIdx];
    while (node != nullptr) {
        if (readU32(node, 0x00) == key) {
            if (node == buckets[bucketCount]) {
                return nullptr;
            }
            return readPtr<void>(node, 0x04);
        }
        node = readPtr<const void>(node, 0x08);
    }
    return nullptr;
}

// Helper matching 0x0078D880 (ThingFactory::findTemplateById on [0x00DF9D28] + 0x28)
void* findTemplateById_78D880(const void* thingFactory, std::uint32_t templateId) noexcept {
    if (thingFactory == nullptr) {
        return nullptr;
    }
    const auto* tableBase = static_cast<const std::uint8_t*>(thingFactory) + 0x28;
    return findInHashTable_50B5F0(tableBase, templateId);
}

// Helper matching 0x0050B5F0 on TheGameLogic + 0xC4
void* findObjectByIdInGameLogic(const void* gameLogic, std::uint32_t objectId) noexcept {
    if (gameLogic == nullptr || objectId == 0u) {
        return nullptr;
    }
    const auto* tableBase = static_cast<const std::uint8_t*>(gameLogic) + 0xC4;
    return findInHashTable_50B5F0(tableBase, objectId);
}

// Helper matching 0x006C3A10 (unsigned key rbtree find on treeBase: +4 = sentinel, +0x0C = root)
const RbTreeNodeView* findRbTreeNodeUnsigned_6C3A10(
    const void* treeBase,
    std::uint32_t key) noexcept {
    const auto* sentinel = reinterpret_cast<const RbTreeNodeView*>(
        static_cast<const std::uint8_t*>(treeBase) + 0x04);
    const auto* cur = readPtr<const RbTreeNodeView>(treeBase, 0x0C);
    const RbTreeNodeView* candidate = sentinel;
    while (cur != nullptr) {
        if (cur->keyAt10 >= key) {
            candidate = cur;
            cur = cur->leftAt4;
        } else {
            cur = cur->rightAt0;
        }
    }
    if (candidate == sentinel || key < candidate->keyAt10) {
        return sentinel;
    }
    return candidate;
}

// Helper matching 0x00AAFE10 (signed key rbtree find on treeBase: +4 = sentinel, +0x0C = root)
const RbTreeNodeView* findRbTreeNodeSigned_AAFE10(
    const void* treeBase,
    std::int32_t key) noexcept {
    const auto* sentinel = reinterpret_cast<const RbTreeNodeView*>(
        static_cast<const std::uint8_t*>(treeBase) + 0x04);
    const auto* cur = readPtr<const RbTreeNodeView>(treeBase, 0x0C);
    const RbTreeNodeView* candidate = sentinel;
    while (cur != nullptr) {
        if (static_cast<std::int32_t>(cur->keyAt10) >= key) {
            candidate = cur;
            cur = cur->leftAt4;
        } else {
            cur = cur->rightAt0;
        }
    }
    if (candidate == sentinel || key < static_cast<std::int32_t>(candidate->keyAt10)) {
        return sentinel;
    }
    return candidate;
}

// Helper matching 0x00506DF0 (64-bit (int32, uint32) key rbtree find on treeBase: +4 = sentinel, +0x0C = root)
const RbTreeNodeView* findRbTreeNode64_506DF0(
    const void* treeBase,
    std::int32_t key0,
    std::uint32_t key1) noexcept {
    const auto* sentinel = reinterpret_cast<const RbTreeNodeView*>(
        static_cast<const std::uint8_t*>(treeBase) + 0x04);
    const auto* cur = readPtr<const RbTreeNodeView>(treeBase, 0x0C);
    const RbTreeNodeView* candidate = sentinel;
    while (cur != nullptr) {
        const std::int32_t n0 = static_cast<std::int32_t>(cur->keyAt10);
        const std::uint32_t n1 = cur->valueAt14;
        const bool nodeLess = (n0 != key0) ? (n0 < key0) : (n1 < key1);
        if (!nodeLess) {
            candidate = cur;
            cur = cur->leftAt4;
        } else {
            cur = cur->rightAt0;
        }
    }
    if (candidate == sentinel) {
        return sentinel;
    }
    const std::int32_t c0 = static_cast<std::int32_t>(candidate->keyAt10);
    const std::uint32_t c1 = candidate->valueAt14;
    const bool keyLess = (key0 != c0) ? (key0 < c0) : (key1 < c1);
    if (keyLess) {
        return sentinel;
    }
    return candidate;
}

// Helper matching 0x009C9980 (rbtree increment)
RbTreeNodeView* incrementRbTreeNode_9C9980(RbTreeNodeView* node) noexcept {
    if (node == nullptr) {
        return nullptr;
    }
    if (readU8(node, 0x2D) != 0u) {
        return node;
    }
    RbTreeNodeView* right = node->rightAt0;
    if (right != nullptr && readU8(right, 0x2D) == 0u) {
        while (right->leftAt4 != nullptr && readU8(right->leftAt4, 0x2D) == 0u) {
            right = right->leftAt4;
        }
        return right;
    }
    RbTreeNodeView* parent = node->parentAt8;
    RbTreeNodeView* cur = node;
    while (parent != nullptr && readU8(parent, 0x2D) == 0u && cur == parent->rightAt0) {
        cur = parent;
        parent = parent->parentAt8;
    }
    return parent;
}

// Helper matching 0x005049E0 (3D distance squared in x87 precision)
float distanceSquared3D_5049E0(const Coord3DView* a, const Coord3DView* b) noexcept {
    const double dx = static_cast<double>(a->x) - static_cast<double>(b->x);
    const double dy = static_cast<double>(a->y) - static_cast<double>(b->y);
    const double dz = static_cast<double>(a->z) - static_cast<double>(b->z);
    return static_cast<float>((dx * dx + dy * dy) + dz * dz);
}

// Helper matching SWAR popcount in 0x00890600
std::int32_t swarPopcount32(std::uint32_t x) noexcept {
    x = x - ((x >> 1u) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2u) & 0x33333333u);
    x = (x + (x >> 4u)) & 0x0F0F0F0Fu;
    return static_cast<std::int32_t>((x * 0x01010101u) >> 24u);
}

// Helper matching 0x0078DA40 (ThingTemplate::isEquivalentTo)
bool isTemplateEquivalent_78DA40(
    const void* tmplA,
    const void* tmplB,
    const void* thingFactory) noexcept {
    if (tmplB == tmplA) {
        return true;
    }
    if (tmplB == nullptr || tmplA == nullptr) {
        return false;
    }
    const void* dataA = readPtr<const void>(tmplA, 0x14);
    if (dataA != nullptr) {
        const std::uint32_t countA = readU32(dataA, 0x1BC);
        const auto* idsA = readPtr<const std::uint32_t>(dataA, 0x1C0);
        if (countA != 0u && idsA != nullptr) {
            for (std::uint32_t i = 0; i < countA; ++i) {
                if (findTemplateById_78D880(thingFactory, idsA[i]) == tmplB) {
                    return true;
                }
            }
        }
    }
    const void* dataB = readPtr<const void>(tmplB, 0x14);
    if (dataB != nullptr) {
        const std::uint32_t countB = readU32(dataB, 0x1BC);
        const auto* idsB = readPtr<const std::uint32_t>(dataB, 0x1C0);
        if (countB != 0u && idsB != nullptr) {
            for (std::uint32_t i = 0; i < countB; ++i) {
                if (findTemplateById_78D880(thingFactory, idsB[i]) == tmplA) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Helper matching 0x008ADD50 (in-place vector insert at end when end < cap)
void insertU32AtEndInPlace_8ADD50(void* vecBase, const std::uint32_t* valPtr) noexcept {
    auto* endPtr = readPtr<std::uint32_t>(vecBase, 0x04);
    auto* capPtr = readPtr<std::uint32_t>(vecBase, 0x08);
    if (endPtr != nullptr && endPtr < capPtr) {
        const std::uint32_t val = *valPtr;
        *endPtr = val;
        writePtr(vecBase, 0x04, endPtr + 1);
    }
}

} // namespace

// 1. 0x00881130 — AIBudgetStateManager::normalizeStateScores
void normalizeBudgetStateScores_881130(AIBudgetStateManagerView* self) noexcept {
    float sum = 0.0f;
    for (AIBudgetStateEntryView* it = self->beginAt4; it != self->endAt8; ++it) {
        if (it->stateAt0 != nullptr) {
            sum += it->scoreAt4;
        }
    }
    if (sum > 0.0f) {
        self->hasPositiveScoreAt14 = 1u;
        for (AIBudgetStateEntryView* it = self->beginAt4; it != self->endAt8; ++it) {
            if (it->stateAt0 != nullptr) {
                it->scoreAt4 = it->scoreAt4 / sum;
            }
        }
    } else {
        self->hasPositiveScoreAt14 = 0u;
        for (AIBudgetStateEntryView* it = self->beginAt4; it != self->endAt8; ++it) {
            if (it->stateAt0 != nullptr) {
                it->scoreAt4 = 0.0f;
            }
        }
    }
}

// 2. 0x00882190 — AIBuildable::resetBuildPosition
void resetBuildPosition_882190(void* /*self*/, Coord3DView* outPos) noexcept {
    outPos->x = 0.0f;
    outPos->y = 0.0f;
    outPos->z = 0.0f;
}

// 3. 0x00882400 — AIBuildableStructure::setBuildPosition
void setBuildPositionWithTerrainHeight_882400(
    void* self,
    const Coord3DView* pos,
    void* terrainLogicOverride) noexcept {
    auto* dst = readPtr<Coord3DView>(self, 0x44);
    dst->x = pos->x;
    dst->y = pos->y;
    dst->z = pos->z;

    void* terrain = resolveTerrainLogic(terrainLogicOverride);
    const auto* vft = readPtr<const std::uintptr_t>(terrain, 0x00);
    using GetGroundHeightFn = float(__fastcall*)(void*, void*, float, float, void*);
    const auto fn = reinterpret_cast<GetGroundHeightFn>(vft[6]); // +0x18
    auto* dstAfter = readPtr<Coord3DView>(self, 0x44);
    const float groundZ = fn(terrain, nullptr, pos->x, pos->y, nullptr);
    dstAfter->z = groundZ;
    writeU8(self, 0x64, 1u);
}

// 4. 0x00882A90 — AIBuildPersonalityConfig::findBitmaskFloatOrZero
float findBitmaskFloatOrZero_882A90(const void* self, std::uint32_t bitIndex) noexcept {
    const void* inner = readPtr<const void>(self, 0x00);
    const auto* base = readPtr<const std::uint8_t>(inner, 0x108);
    const std::uint32_t count = readU32(inner, 0x104);
    if (count == 0u) {
        return 0.0f;
    }
    const std::uint32_t bitMask = 1u << (bitIndex & 31u);
    const std::ptrdiff_t maskOffset = static_cast<std::ptrdiff_t>((bitIndex >> 5u) * 4u + 4u);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint8_t* entry = base + i * 8u;
        if ((readU32(entry, maskOffset) & bitMask) != 0u) {
            return readF32(entry, 0x00);
        }
    }
    return 0.0f;
}

// 5. 0x00882AE0 — AIBuildPersonalityConfig::findEntry20ByBit
const void* findEntry20ByBit_882AE0(const void* self, std::uint32_t bitIndex) noexcept {
    const void* inner = readPtr<const void>(self, 0x00);
    const auto* base = readPtr<const std::uint8_t>(inner, 0x110);
    const std::uint32_t count = readU32(inner, 0x10C);
    if (count == 0u) {
        return nullptr;
    }
    const std::uint32_t bitMask = 1u << (bitIndex & 31u);
    const std::ptrdiff_t maskOffset = static_cast<std::ptrdiff_t>((bitIndex >> 5u) * 4u + 0x10u);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint8_t* entry = base + i * 20u;
        if ((readU32(entry, maskOffset) & bitMask) != 0u) {
            return entry;
        }
    }
    return nullptr;
}

// 6. 0x00882C30 — AIBuildPersonalityConfig::findBitmaskFloatOrOne
float findBitmaskFloatOrOne_882C30(const void* self, std::uint32_t bitIndex) noexcept {
    const void* inner = readPtr<const void>(self, 0x00);
    const auto* base = readPtr<const std::uint8_t>(inner, 0x130);
    const std::uint32_t count = readU32(inner, 0x12C);
    if (count == 0u) {
        return 1.0f;
    }
    const std::uint32_t bitMask = 1u << (bitIndex & 31u);
    const std::ptrdiff_t maskOffset = static_cast<std::ptrdiff_t>((bitIndex >> 5u) * 4u + 4u);
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint8_t* entry = base + i * 8u;
        if ((readU32(entry, maskOffset) & bitMask) != 0u) {
            return readF32(entry, 0x00);
        }
    }
    return 1.0f;
}

// 7. 0x00884550 — AIBaseBuilder::updateBasePriorityFlags
void updateBasePriorityFlags_884550(void* self) noexcept {
    auto** begin = readPtr<AIBaseView*>(self, 0x28);
    auto** end = readPtr<AIBaseView*>(self, 0x2C);
    if (begin == end) {
        return;
    }

    AIBaseView* oldMin[3] = { nullptr, nullptr, nullptr };
    AIBaseView* oldMax[3] = { nullptr, nullptr, nullptr };
    AIBaseView* newMin[3] = { nullptr, nullptr, nullptr };
    AIBaseView* newMax[3] = { nullptr, nullptr, nullptr };

    for (AIBaseView** it = begin; it != end; ++it) {
        AIBaseView* base = *it;
        if (base == nullptr) {
            continue;
        }
        std::uint32_t cat = 0u;
        while (cat <= 2u) {
            if ((base->flagsAt2C & (1u << cat)) != 0u) {
                break;
            }
            ++cat;
        }
        if (cat > 2u) {
            continue;
        }

        const std::uint32_t flags = base->flagsAt2C;
        if (((flags >> 7u) & 1u) != 0u) {
            oldMin[cat] = base;
        }
        if (((flags >> 8u) & 1u) != 0u) {
            oldMax[cat] = base;
        }
        if (newMin[cat] == nullptr || newMin[cat]->baseIdAt4 > base->baseIdAt4) {
            newMin[cat] = base;
        }
        if (newMax[cat] == nullptr || newMax[cat]->baseIdAt4 < base->baseIdAt4) {
            newMax[cat] = base;
        }
    }

    for (std::uint32_t cat = 0u; cat < 3u; ++cat) {
        if (oldMin[cat] != newMin[cat]) {
            if (oldMin[cat] != nullptr) {
                oldMin[cat]->flagsAt2C &= ~0x80u;
            }
            if (newMin[cat] != nullptr) {
                newMin[cat]->flagsAt2C |= 0x80u;
            }
        }
        if (oldMax[cat] != newMax[cat]) {
            if (oldMax[cat] != nullptr) {
                oldMax[cat]->flagsAt2C &= ~0x100u;
            }
            if (newMax[cat] != nullptr) {
                newMax[cat]->flagsAt2C |= 0x100u;
            }
        }
    }
}

// 8. 0x00884C40 — AIBuilder::hasActiveBuilderAtPosition
bool hasActiveBuilderAtPosition_884C40(const void* self, const Coord3DView* pos) noexcept {
    const void* ctx = readPtr<const void>(self, 0x264);
    const auto* aiPlayer = readPtr<const std::uint8_t>(ctx, 0x90);
    const auto* vecBase = aiPlayer + 0x3EC;
    auto* begin = readPtr<const std::uint8_t>(vecBase, 0x0C);
    auto* end = readPtr<const std::uint8_t>(vecBase, 0x10);
    const std::ptrdiff_t initialCount = (end - begin) / 12;
    if (initialCount == 0) {
        return false;
    }

    void* builder = readPtr<void>(begin, 0x00);
    if (builder == nullptr) {
        return false;
    }

    const std::uint32_t targetX = readU32(pos, 0x00);
    const std::uint32_t targetY = readU32(pos, 0x04);

    std::size_t idx = 0u;
    std::ptrdiff_t offset = 0;
    for (;;) {
        begin = readPtr<const std::uint8_t>(vecBase, 0x0C);
        end = readPtr<const std::uint8_t>(vecBase, 0x10);
        const std::uint8_t* found = end;
        for (const std::uint8_t* scan = begin; scan != end; scan += 12) {
            if (readPtr<void>(scan, 0x00) == builder) {
                found = scan;
                break;
            }
        }

        if (found != end && readU32(found, 0x04) != 0u) {
            using GetTaskFn = const void*(__fastcall*)(void*, void*);
            const auto* vft0 = readPtr<const std::uintptr_t>(builder, 0x00);
            const auto getTask0 = reinterpret_cast<GetTaskFn>(vft0[29]); // +0x74
            if (getTask0(builder, nullptr) != nullptr) {
                const auto* vft1 = readPtr<const std::uintptr_t>(builder, 0x00);
                const auto getTask1 = reinterpret_cast<GetTaskFn>(vft1[29]);
                const void* task1 = getTask1(builder, nullptr);
                const void* posObj14 = readPtr<const void>(task1, 0x14);
                if (posObj14 != nullptr
                    && readU32(posObj14, 0x04) == targetX
                    && readU32(posObj14, 0x08) == targetY) {
                    return true;
                }

                const auto* vft2 = readPtr<const std::uintptr_t>(builder, 0x00);
                const auto getTask2 = reinterpret_cast<GetTaskFn>(vft2[29]);
                const void* task2 = getTask2(builder, nullptr);
                const void* posObj10 = readPtr<const void>(task2, 0x10);
                if (posObj10 != nullptr
                    && readU32(posObj10, 0x04) == targetX
                    && readU32(posObj10, 0x08) == targetY) {
                    return true;
                }
            }
        }

        begin = readPtr<const std::uint8_t>(vecBase, 0x0C);
        end = readPtr<const std::uint8_t>(vecBase, 0x10);
        const std::size_t count = static_cast<std::size_t>((end - begin) / 12);
        ++idx;
        offset += 12;
        if (idx >= count) {
            break;
        }
        builder = readPtr<void>(begin, offset);
        if (builder == nullptr) {
            break;
        }
    }
    return false;
}

// 9. 0x00886320 — AIBudgetState::evaluateHeuristicProduct
float evaluateHeuristicProduct_886320(
    const AIBudgetStateView* self,
    void* contextArg0,
    void* contextArg1) noexcept {
    const auto* sentinel = reinterpret_cast<const AIHeuristicCallNodeView*>(
        reinterpret_cast<const std::uint8_t*>(self) + 0x04);
    const AIHeuristicCallNodeView* first = self->nextAt4;
    float product = (first == sentinel) ? 0.0f : 1.0f;

    const AIHeuristicCallNodeView* cur = first;
    while (cur != sentinel) {
        if (!(product > 0.0f)) {
            break;
        }
        void* heur = cur->heuristicAt8;
        if (heur != nullptr) {
            const auto* vft = readPtr<const std::uintptr_t>(heur, 0x00);
            using EvalFn = float(__fastcall*)(void*, void*, void*, void*);
            const auto evalFn = reinterpret_cast<EvalFn>(vft[1]); // +0x04
            const float raw = evalFn(heur, nullptr, contextArg0, contextArg1);
            const float clamped = (0.0f > raw) ? 0.0f : raw;
            product = product * clamped;
            if (product == 0.0f) {
                break;
            }
        }
        cur = cur->nextAt0;
    }
    return product;
}

// 10. 0x00887170 — AIBuildableQueue::popNextTemplateId
std::uint32_t popNextTemplateId_887170(AIBuildableQueueView* self) noexcept {
    if (self->enabledAt0 == 0u) {
        return 0xFFFFFFFFu;
    }
    std::uint32_t* cursor = self->cursorAt4;
    std::uint32_t* base = self->baseAt8;
    if (cursor == base || self->exhaustedAt0C != 0u) {
        return 0xFFFFFFFFu;
    }
    std::uint32_t* prev = cursor - 2;
    const std::uint32_t result = *(cursor - 1);
    const std::uint32_t prevKey = *prev;
    if (prev == base) {
        self->cursorAt4 = prev;
        self->exhaustedAt0C = 1u;
        return result;
    }
    if (*(prev - 2) != prevKey) {
        self->exhaustedAt0C = 1u;
        return result;
    }
    self->cursorAt4 = prev;
    return result;
}

// 11. 0x00887300 — AIBuildableRegistry::findEntryByTemplateId
void* findEntryByTemplateId_887300(const void* self, std::uint32_t templateId) noexcept {
    auto* base = readPtr<std::uint8_t>(self, 0x1C);
    const std::uint32_t count = readU32(self, 0x20);
    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint8_t* entry = base + i * 0x2CCu;
        if (readU32(entry, 0x24) == templateId) {
            return entry;
        }
    }
    return nullptr;
}

// 12. 0x008878C0 — AIBuildableRegistry::lookupNestedValue
std::uint32_t lookupNestedValue_8878C0(
    const void* self,
    std::int32_t outerKey,
    std::int32_t innerKey) noexcept {
    const auto* outerTree = static_cast<const std::uint8_t*>(self) + 0x04;
    const auto* outerSentinel = reinterpret_cast<const RbTreeNodeView*>(
        static_cast<const std::uint8_t*>(self) + 0x08);
    const RbTreeNodeView* outerNode = findRbTreeNodeSigned_AAFE10(outerTree, outerKey);
    if (outerNode == outerSentinel) {
        return 0u;
    }
    const auto* innerTree = reinterpret_cast<const std::uint8_t*>(outerNode) + 0x14;
    const auto* innerSentinel = reinterpret_cast<const RbTreeNodeView*>(
        reinterpret_cast<const std::uint8_t*>(outerNode) + 0x18);
    const RbTreeNodeView* innerNode = findRbTreeNodeSigned_AAFE10(innerTree, innerKey);
    if (innerNode == innerSentinel) {
        return 0u;
    }
    return innerNode->valueAt14;
}

// 13. 0x00887D60 — AIBuildableEntry::init
void* initBuildableEntry_887D60(
    void* self,
    std::uint32_t ownerAt50,
    std::uint32_t templateAt54) noexcept {
    writeU32(self, 0x00, kVtableAIBuildableEntry);
    writeF32(self, 0x04, 0.0f);
    writeF32(self, 0x08, 0.0f);
    writeF32(self, 0x0C, 0.0f);
    writeF32(self, 0x10, 0.0f);
    writeU32(self, 0x14, 0u);
    writeF32(self, 0x18, 0.0f);
    writeF32(self, 0x1C, 0.0f);
    writeF32(self, 0x20, 0.0f);
    writeF32(self, 0x24, 0.0f);
    writeF32(self, 0x28, 0.0f);
    writeF32(self, 0x2C, 100.0f);
    writeU32(self, 0x30, 0u);
    writeU32(self, 0x34, 0u);
    writeU8(self, 0x38, 1u);
    writeU32(self, 0x3C, 0u);
    writeU32(self, 0x40, 1u);
    writeF32(self, 0x44, 0.0f);
    writeF32(self, 0x48, 0.0f);
    writeU8(self, 0x4C, 0u);
    writeU8(self, 0x4D, 0u);
    writeU32(self, 0x50, ownerAt50);
    writeU32(self, 0x54, templateAt54);
    return self;
}

// 14. 0x0088A0E0 — AIBankAccount::sumWeightedRequestsAbovePriority
float sumWeightedRequestsAbovePriority_88A0E0(
    const AIBankAccountView* self,
    float minPriority,
    float* inOutMaxPriority) noexcept {
    const auto* sentinel = reinterpret_cast<const AIBankRequestNodeView*>(
        reinterpret_cast<const std::uint8_t*>(self) + 0x10);
    if (self->requestListNextAt10 == sentinel) {
        return 0.0f;
    }

    const AIBankRequestNodeView* cur = self->requestListNextAt10;
    while (cur != sentinel) {
#if defined(RA3_SKIRMISHAIBASEBUILD_ORACLE_MUTANT)
        if (!(minPriority >= cur->priorityAt14)) {
#else
        if (!(minPriority > cur->priorityAt14)) {
#endif
            break;
        }
        cur = cur->nextAt0;
    }

    const float initCap = *inOutMaxPriority;
    if (0.0f >= initCap || minPriority > initCap) {
        *inOutMaxPriority = self->requestListPrevAt14->priorityAt14;
    }

    float sum = 0.0f;
    while (cur != sentinel) {
        const float cap = *inOutMaxPriority;
        const float prio = cur->priorityAt14;
        const float clamped = (cap > prio) ? prio : cap;
        sum += (clamped - minPriority) * cur->costAt18;
        cur = cur->nextAt0;
    }
    return sum;
}

// 15. 0x0088AD80 — AIBuilder::sumQueuedPowerDelta
std::int32_t sumQueuedPowerDelta_88AD80(
    const void* self,
    const void* thingFactoryOverride) noexcept {
    const void* factory = resolveThingFactory(thingFactoryOverride);
    const auto* reg = readPtr<const std::uint8_t>(self, 0x538);
    const auto* begin = readPtr<const std::uint32_t>(self, 0x53C);
    const auto* end = readPtr<const std::uint32_t>(self, 0x540);
    std::int32_t totalPower = 0;

    for (const std::uint32_t* it = begin; it != end; ++it) {
        const std::uint32_t key = *it;
        const auto* sentinel = reinterpret_cast<const RbTreeNodeView*>(reg + 0x0C);
        const auto* node = findRbTreeNodeUnsigned_6C3A10(reg + 0x08, key);
        const void* entry = (node != sentinel)
            ? reinterpret_cast<const void*>(static_cast<std::uintptr_t>(node->valueAt14))
            : nullptr;
        const std::uint32_t tmplId = readU32(entry, 0x10);
        const void* tmpl = findTemplateById_78D880(factory, tmplId);
        if (tmpl != nullptr) {
            const void* tmplData = readPtr<const void>(tmpl, 0x14);
            totalPower += readI32(tmplData, 0x8C);
        }
        end = readPtr<const std::uint32_t>(self, 0x540);
    }
    return totalPower;
}

// 16. 0x0088AE10 — AIBuilder::countQueuedTemplate
std::int32_t countQueuedTemplate_88AE10(const void* self, std::uint32_t templateId) noexcept {
    const auto* reg = readPtr<const std::uint8_t>(self, 0x538);
    const auto* begin = readPtr<const std::uint32_t>(self, 0x53C);
    const auto* end = readPtr<const std::uint32_t>(self, 0x540);
    std::int32_t count = 0;

    for (const std::uint32_t* it = begin; it != end; ++it) {
        const std::uint32_t key = *it;
        const auto* sentinel = reinterpret_cast<const RbTreeNodeView*>(reg + 0x0C);
        const auto* node = findRbTreeNodeUnsigned_6C3A10(reg + 0x08, key);
        const void* entry = (node != sentinel)
            ? reinterpret_cast<const void*>(static_cast<std::uintptr_t>(node->valueAt14))
            : nullptr;
        if (readU32(entry, 0x10) == templateId) {
            ++count;
        }
    }
    return count;
}

// 17. 0x0088AFC0 — AIBuilder::notifyObjectConstructed
void notifyObjectConstructed_88AFC0(
    void* self,
    std::uint32_t builderId,
    const std::uint32_t* templateIdPtr) noexcept {
    const auto* reg = readPtr<const std::uint8_t>(self, 0x538);
    const auto* begin = readPtr<const std::uint32_t>(self, 0x53C);
    const auto* end = readPtr<const std::uint32_t>(self, 0x540);

    for (const std::uint32_t* it = begin; it != end; ++it) {
        const std::uint32_t key = *it;
        const auto* sentinel = reinterpret_cast<const RbTreeNodeView*>(reg + 0x0C);
        const auto* node = findRbTreeNodeUnsigned_6C3A10(reg + 0x08, key);
        void* entry = (node != sentinel)
            ? reinterpret_cast<void*>(static_cast<std::uintptr_t>(node->valueAt14))
            : nullptr;
        if (readU32(entry, 0x0C) == builderId && readU32(entry, 0x10) == *templateIdPtr) {
            const std::int32_t state = readI32(entry, 0x14);
            if (state != 5 && state != 6 && state != 4) {
                const auto* vft = readPtr<const std::uintptr_t>(entry, 0x00);
                using NotifyFn = void(__fastcall*)(void*, void*, void*);
                const auto fn = reinterpret_cast<NotifyFn>(vft[17]); // +0x44
                fn(entry, nullptr, readPtr<void>(self, 0x264));
            }
            return;
        }
    }
}

// 18. 0x0088BC10 — AIEconomyStatsSubobject::initWithOwner
void* initEconomyStatsWithOwner_88BC10(
    void* self,
    void* ownerAt8,
    void* ownerAt4) noexcept {
    auto* bytes = static_cast<std::uint8_t*>(self);
    writeU32(self, 0x00, kVtableAIEconomyStatsSubobject);
    writePtr(self, 0x04, ownerAt4);
    writePtr(self, 0x08, ownerAt8);
    writeU32(self, 0x0C, 0u);
    writeU32(self, 0x10, 0u);
    writeU32(self, 0x14, 0u);
    writePtr(self, 0x1C, bytes + 0x1C);
    writePtr(self, 0x20, bytes + 0x1C);
    writeU32(self, 0x24, 0u);
    writeU32(self, 0x28, 0u);
    writeU32(self, 0x2C, 0u);
    writeU32(self, 0x34, 0u);
    writeU32(self, 0x38, 0u);
    writeU32(self, 0x3C, 0u);
    writePtr(self, 0x48, bytes + 0x48);
    writePtr(self, 0x4C, bytes + 0x48);
    writeU32(self, 0x50, 0u);
    writeU32(self, 0x54, 0u);
    writeU32(self, 0x58, 0u);
    return self;
}

// 19. 0x0088BE50 — AIEconomyStats::findStatValueByKey
std::uint32_t findStatValueByKey_88BE50(const void* self, std::uint32_t key) noexcept {
    const void* target = readPtr<const void>(self, 0x08);
    if (target == nullptr) {
        target = self;
    }
    const auto* treeBase = static_cast<const std::uint8_t*>(target) + 0x44;
    const auto* sentinel = reinterpret_cast<const RbTreeNodeView*>(
        static_cast<const std::uint8_t*>(target) + 0x48);
    const RbTreeNodeView* node = findRbTreeNodeUnsigned_6C3A10(treeBase, key);
    if (node == sentinel) {
        return 0u;
    }
    return node->valueAt14;
}

// 20. 0x0088BE90 — AIEconomyStatsSlidingWindow::pruneExpiredSamples
void pruneExpiredSamples_88BE90(
    void* self,
    const void* gameLogicOverride) noexcept {
    const void* gameLogic = resolveGameLogic(gameLogicOverride);
    const std::uint32_t currentFrame = readU32(gameLogic, 0x58);
    const std::uint32_t windowFrames = readU32(self, 0x34);
    const std::uint32_t minFrame =
        (currentFrame > windowFrames) ? (currentFrame - windowFrames) : 0u;

    while (readPtr<EconomySampleView>(self, 0x08) != readPtr<EconomySampleView>(self, 0x18)) {
        auto* head = readPtr<EconomySampleView>(self, 0x08);
        if (head->frameAt0 >= minFrame) {
            break;
        }
        const float newSum = readF32(self, 0x2C) - head->valueAt4;
        EconomySampleView* next = head + 1;
        writeF32(self, 0x2C, newSum);
        if (next != readPtr<EconomySampleView>(self, 0x10)) {
            writePtr(self, 0x08, next);
        } else {
            auto** blockSlot = readPtr<EconomySampleView*>(self, 0x14) + 1;
            writePtr(self, 0x14, blockSlot);
            EconomySampleView* newBlock = *blockSlot;
            writePtr(self, 0x0C, newBlock);
            writePtr(self, 0x10, reinterpret_cast<EconomySampleView*>(
                reinterpret_cast<std::uint8_t*>(newBlock) + 0x100));
            writePtr(self, 0x08, newBlock);
        }
    }
    if (0.0f > readF32(self, 0x2C)) {
        writeF32(self, 0x2C, 0.0f);
    }
}

// 21. 0x0088D120 — AIBuildableGroup::containsTemplateId
bool groupContainsTemplateId_88D120(const void* self, std::uint32_t templateId) noexcept {
    const auto* owner = readPtr<const std::uint8_t>(self, 0x0C);
    const auto* begin = readPtr<const std::uint32_t>(self, 0x10);
    const auto* end = readPtr<const std::uint32_t>(self, 0x14);
    const auto* sentinel = reinterpret_cast<const RbTreeNodeView*>(owner + 0x0C);

    for (const std::uint32_t* it = begin; it != end; ++it) {
        const RbTreeNodeView* node = findRbTreeNodeUnsigned_6C3A10(owner + 0x08, *it);
        const void* entry = (node != sentinel)
            ? reinterpret_cast<const void*>(static_cast<std::uintptr_t>(node->valueAt14))
            : nullptr;
        if (readU32(entry, 0x10) == templateId) {
            return true;
        }
    }
    return false;
}

// 22. 0x0088D1A0 — AIBuildableCategoryMap::containsTemplateId
bool categoryMapContainsTemplateId_88D1A0(const void* self, std::uint32_t templateId) noexcept {
    auto* outerEnd = reinterpret_cast<RbTreeNodeView*>(
        const_cast<std::uint8_t*>(static_cast<const std::uint8_t*>(self) + 0x08));
    RbTreeNodeView* outerCur = readPtr<RbTreeNodeView>(self, 0x0C);
    while (outerCur != outerEnd) {
        auto* innerEnd = reinterpret_cast<RbTreeNodeView*>(
            reinterpret_cast<std::uint8_t*>(outerCur) + 0x18);
        RbTreeNodeView* innerCur = outerCur->nestedLeftmostAt1C;
        while (innerCur != innerEnd) {
            const void* group = reinterpret_cast<const void*>(
                static_cast<std::uintptr_t>(innerCur->valueAt14));
            if (groupContainsTemplateId_88D120(group, templateId)) {
                return true;
            }
            innerCur = incrementRbTreeNode_9C9980(innerCur);
        }
        outerCur = incrementRbTreeNode_9C9980(outerCur);
    }
    return false;
}

// 23. 0x0088DE50 — AIBuildableIterator::advanceToNextActive
void advanceToNextActive_88DE50(AIBuildableIteratorView* self) noexcept {
    if (self->ownerMapAt8 == nullptr) {
        return;
    }
    RbTreeNodeView* cur = incrementRbTreeNode_9C9980(self->nodeAt0);
    self->nodeAt0 = cur;

    auto* ownerBytes = static_cast<std::uint8_t*>(self->ownerMapAt8) + 0x04;
    auto* sentinel = reinterpret_cast<RbTreeNodeView*>(ownerBytes + 0x04);
    RbTreeNodeView* upperBound = sentinel;
    RbTreeNodeView* scan = readPtr<RbTreeNodeView>(ownerBytes, 0x0C);
    const std::uint32_t key = self->keyAt4;
    while (scan != nullptr) {
        if (key < scan->keyAt10) {
            upperBound = scan;
            scan = scan->leftAt4;
        } else {
            scan = scan->rightAt0;
        }
    }

    while (self->nodeAt0 != upperBound) {
        if (self->nodeAt0->blockedFlagAt28 == 0u) {
            return;
        }
        self->nodeAt0 = incrementRbTreeNode_9C9980(self->nodeAt0);
    }
    self->ownerMapAt8 = nullptr;
}

// 24. 0x0088DEC0 — AIBuildableCategoryMap::findFirstActiveForKey
AIBuildableIteratorView* findFirstActiveForKey_88DEC0(
    void* self,
    AIBuildableIteratorView* outIter,
    std::uint32_t key) noexcept {
    outIter->nodeAt0 = nullptr;
    outIter->keyAt4 = 0u;
    outIter->ownerMapAt8 = nullptr;

    auto* sentinel = reinterpret_cast<RbTreeNodeView*>(
        static_cast<std::uint8_t*>(self) + 0x08);
    RbTreeNodeView* root = readPtr<RbTreeNodeView>(self, 0x10);
    RbTreeNodeView* lowerBound = sentinel;
    RbTreeNodeView* upperBound = sentinel;

    RbTreeNodeView* scan = root;
    while (scan != nullptr) {
        if (scan->keyAt10 >= key) {
            lowerBound = scan;
            scan = scan->leftAt4;
        } else {
            scan = scan->rightAt0;
        }
    }

    scan = root;
    while (scan != nullptr) {
        if (key < scan->keyAt10) {
            upperBound = scan;
            scan = scan->leftAt4;
        } else {
            scan = scan->rightAt0;
        }
    }

    if (lowerBound == upperBound) {
        return outIter;
    }

    RbTreeNodeView* cur = lowerBound;
    while (cur != upperBound) {
        if (cur->blockedFlagAt28 == 0u) {
            outIter->nodeAt0 = cur;
            outIter->keyAt4 = key;
            outIter->ownerMapAt8 = self;
            return outIter;
        }
        cur = incrementRbTreeNode_9C9980(cur);
    }
    return outIter;
}

// 25. 0x008900B0 — AIBankAccount::canAffordRequest
bool canAffordRequest_8900B0(
    const AIBankAccountView* self,
    float requestCost,
    float requestPriority,
    float rateScale) noexcept {
    const float baseIncome = requestPriority * rateScale * self->budgetWeightAt0C;
    float maxPrio1 = requestPriority;
    const float higherCost1 = sumWeightedRequestsAbovePriority_88A0E0(
        self, 0.0f, &maxPrio1);
    const float needed1 = (self->spentAt1C + higherCost1) + requestCost;
    const float budget1 = baseIncome + self->availableAt20;
    if (needed1 > budget1) {
        return false;
    }

    float maxPrio2 = 0.0f;
    const float higherCost2 = sumWeightedRequestsAbovePriority_88A0E0(
        self, requestPriority, &maxPrio2);
    if (requestPriority >= maxPrio2) {
        return true;
    }

    const float needed2 = ((self->spentAt1C + higherCost2) + higherCost1) + requestCost;
    const float extraIncome = (maxPrio2 - requestPriority) * rateScale * self->budgetWeightAt0C;
    const float budget2 = (extraIncome + baseIncome) + self->availableAt20;
    return !(needed2 > budget2);
}

// 26. 0x00890400 — AIBase::getPosition
const Coord3DView* getBasePosition_890400(
    const AIBaseView* self,
    const void* gameLogicOverride) noexcept {
    const void* gameLogic = resolveGameLogic(gameLogicOverride);
    const std::uint32_t objId = self->centerObjectIdAt0C;
    if (objId != 0u) {
        const void* obj = findObjectByIdInGameLogic(gameLogic, objId);
        if (obj != nullptr) {
            return reinterpret_cast<const Coord3DView*>(
                static_cast<const std::uint8_t*>(obj) + 0x38);
        }
    }
    return &self->fallbackPositionAt10;
}

// 27. 0x00890600 — AIBaseQueryCriteria::isCandidateBetterThanBest
bool isCandidateBetterThanBest_890600(
    const AIBaseQueryCriteriaView* self,
    const AIBaseView* candidateBase,
    const AIBaseView* currentBestBase,
    const void* gameLogicOverride) noexcept {
    if (candidateBase == nullptr) {
        return false;
    }
    if (currentBestBase == nullptr) {
        return true;
    }

    const std::uint32_t candFlags = candidateBase->flagsAt2C;
    const std::uint32_t bestFlags = currentBestBase->flagsAt2C;
    const std::int32_t candScore = swarPopcount32(self->preferredMaskAt8 & candFlags)
        - swarPopcount32(self->dislikedMaskAtC & candFlags);
    const std::int32_t bestScore = swarPopcount32(self->preferredMaskAt8 & bestFlags)
        - swarPopcount32(self->dislikedMaskAtC & bestFlags);

    if (candScore > bestScore) {
        return true;
    }
    if (candScore < bestScore) {
        return false;
    }

    switch (self->sortModeAt10) {
    case 0u:
        return candidateBase->baseIdAt4 < currentBestBase->baseIdAt4;
    case 1u:
        return candidateBase->baseIdAt4 > currentBestBase->baseIdAt4;
    case 2u: {
        const std::ptrdiff_t candCount =
            candidateBase->structuresEndAt34 - candidateBase->structuresBeginAt30;
        const std::ptrdiff_t bestCount =
            currentBestBase->structuresEndAt34 - currentBestBase->structuresBeginAt30;
        if (candCount != bestCount) {
            return static_cast<std::uint32_t>(candCount) < static_cast<std::uint32_t>(bestCount);
        }
        return candidateBase->baseIdAt4 > currentBestBase->baseIdAt4;
    }
    case 3u: {
        const std::ptrdiff_t candCount =
            candidateBase->structuresEndAt34 - candidateBase->structuresBeginAt30;
        const std::ptrdiff_t bestCount =
            currentBestBase->structuresEndAt34 - currentBestBase->structuresBeginAt30;
        if (candCount != bestCount) {
            return static_cast<std::uint32_t>(bestCount) < static_cast<std::uint32_t>(candCount);
        }
        return candidateBase->baseIdAt4 < currentBestBase->baseIdAt4;
    }
    case 4u:
        if (candidateBase->metricAt40 != currentBestBase->metricAt40) {
            return currentBestBase->metricAt40 < candidateBase->metricAt40;
        }
        return candidateBase->baseIdAt4 > currentBestBase->baseIdAt4;
    case 5u:
        if (candidateBase->metricAt40 != currentBestBase->metricAt40) {
            return candidateBase->metricAt40 < currentBestBase->metricAt40;
        }
        return candidateBase->baseIdAt4 < currentBestBase->baseIdAt4;
    case 6u:
        if (self->referencePosAt14 != nullptr) {
            const Coord3DView* candPos = getBasePosition_890400(candidateBase, gameLogicOverride);
            const float candDistSq = distanceSquared3D_5049E0(self->referencePosAt14, candPos);
            const Coord3DView* bestPos = getBasePosition_890400(currentBestBase, gameLogicOverride);
            const float bestDistSq = distanceSquared3D_5049E0(self->referencePosAt14, bestPos);
            if (candDistSq != bestDistSq) {
                return bestDistSq > candDistSq;
            }
        }
        return candidateBase->baseIdAt4 < currentBestBase->baseIdAt4;
    default:
        return candidateBase->baseIdAt4 > currentBestBase->baseIdAt4;
    }
}

// 28. 0x008908C0 — AIBaseBuilder::findBestMatchingBase
AIBaseView* findBestMatchingBase_8908C0(
    const void* self,
    const AIBaseQueryCriteriaConfigView* config,
    const Coord3DView* referencePos,
    const void* gameLogicOverride) noexcept {
    AIBaseQueryCriteriaView criteria{};
    if (config == nullptr) {
        criteria.requiredMaskAt0 = 0u;
        criteria.forbiddenMaskAt4 = 4u;
        criteria.preferredMaskAt8 = 0u;
        criteria.dislikedMaskAtC = 0u;
        criteria.sortModeAt10 = 1u;
    } else {
        criteria.requiredMaskAt0 = config->requiredMaskAt0;
        criteria.forbiddenMaskAt4 = config->forbiddenMaskAt4;
        criteria.preferredMaskAt8 = config->preferredMaskAt8;
        criteria.dislikedMaskAtC = config->dislikedMaskAtC;
        criteria.sortModeAt10 = config->sortModeAt10;
    }
    criteria.referencePosAt14 = referencePos;

    auto** begin = readPtr<AIBaseView*>(self, 0x28);
    auto** end = readPtr<AIBaseView*>(self, 0x2C);
    AIBaseView* best = nullptr;

    for (AIBaseView** it = begin; it != end; ++it) {
        AIBaseView* candidate = *it;
        const std::uint32_t flags = candidate->flagsAt2C;
        if ((flags & criteria.forbiddenMaskAt4) != 0u) {
            continue;
        }
        if ((flags & criteria.requiredMaskAt0) != criteria.requiredMaskAt0) {
            continue;
        }
        if (isCandidateBetterThanBest_890600(&criteria, candidate, best, gameLogicOverride)) {
            best = candidate;
        }
    }
    return best;
}

// 29. 0x008925B0 — AIBuildState::claimFirstMatchingBuilderSlot
std::uint32_t claimFirstMatchingBuilderSlot_8925B0(
    void* self,
    const void* gameLogicOverride) noexcept {
    const void* gameLogic = resolveGameLogic(gameLogicOverride);
    auto* begin = readPtr<std::uint8_t>(self, 0x0C);
    auto* end = readPtr<std::uint8_t>(self, 0x10);

    for (std::uint8_t* slot = begin; slot != end; slot += 0x0C) {
        const std::uint32_t objId = readU32(slot, 0x04);
        const void* obj = findObjectByIdInGameLogic(gameLogic, objId);
        if (readU8(slot, 0x0A) == 0u && readU8(slot, 0x0B) == 0u && obj != nullptr) {
            const void* producerRef = readPtr<const void>(obj, 0x418);
            const std::uint32_t expectedTeam = readU32(producerRef, 0xAC);

            std::uint32_t actualTeam = expectedTeam;
            const void* moduleAt414 = readPtr<const void>(obj, 0x414);
            if (moduleAt414 != nullptr) {
                const std::uint32_t teamOverride = readU32(moduleAt414, 0x1F8);
                if (teamOverride != 0u) {
                    actualTeam = teamOverride;
                }
            }
            if (actualTeam == expectedTeam) {
                const std::uint32_t claimedId = readU32(slot, 0x00);
                writeU8(slot, 0x0B, 1u);
                return claimedId;
            }
        }
    }
    return 0u;
}

// 30. 0x00892670 — AIBaseBuilder::hasEnoughPowerForTemplate
bool hasEnoughPowerForTemplate_892670(
    const void* self,
    const void* buildRequest,
    const void* thingFactoryOverride,
    const void* powerOverrideMgrOverride) noexcept {
    const void* factory = resolveThingFactory(thingFactoryOverride);
    const std::uint32_t reqTemplateId = readU32(buildRequest, 0x10);
    const void* owner14 = readPtr<const void>(self, 0x14);
    const void* aiBuilderOwner = readPtr<const void>(owner14, 0x90);
    const void* reqTmpl = findTemplateById_78D880(factory, reqTemplateId);
    if (reqTmpl == nullptr) {
        return true;
    }
    const void* reqTmplData = readPtr<const void>(reqTmpl, 0x14);
    if (readI32(reqTmplData, 0x8C) >= 0) {
        return true;
    }

    const void* powerState = readPtr<const void>(owner14, 0x74);
    const auto* sentinelAt20 = static_cast<const std::uint8_t*>(powerState) + 0x20;
    const std::int32_t produced =
        (readPtr<const std::uint8_t>(powerState, 0x20) == sentinelAt20)
            ? readI32(powerState, 0x04)
            : 0;
    const std::int32_t excessPower = produced - readI32(powerState, 0x08);

    const void* powerOverrideMgr = resolvePowerOverrideMgr(powerOverrideMgrOverride);
    if (powerOverrideMgr != nullptr) {
        const auto* treeBase = static_cast<const std::uint8_t*>(powerOverrideMgr) + 0x24;
        const auto* sentinel = reinterpret_cast<const RbTreeNodeView*>(
            static_cast<const std::uint8_t*>(powerOverrideMgr) + 0x28);
        const std::int32_t playerKey = readI32(owner14, 0x24);
        const RbTreeNodeView* node = findRbTreeNode64_506DF0(
            treeBase, playerKey, reqTemplateId);
        const void* overrideEntry = (node != sentinel)
            ? reinterpret_cast<const void*>(static_cast<std::uintptr_t>(node->auxValueAt18))
            : nullptr;
        if (overrideEntry != nullptr) {
            const void* sub8 = readPtr<const void>(overrideEntry, 0x08);
            if (sub8 != nullptr) {
                (void)findTemplateById_78D880(factory, readU32(sub8, 0x18));
            }
        }
    }

    if (excessPower < 0) {
        return false;
    }
    const std::int32_t queuedPowerDelta = sumQueuedPowerDelta_88AD80(
        static_cast<const std::uint8_t*>(aiBuilderOwner) + 0x104,
        factory);
    return (queuedPowerDelta + excessPower) >= 0;
}

// 31. 0x00893520 — AIBuildState::shouldEstablishNewBase
bool shouldEstablishNewBase_893520(
    const void* self,
    const void* gameLogicOverride) noexcept {
    if (readI32(self, 0x198) == 1) {
        const float dz = readF32(self, 0x1AC) - readF32(self, 0x250);
        const float dy = readF32(self, 0x1A8) - readF32(self, 0x24C);
        const float dx = readF32(self, 0x1A4) - readF32(self, 0x248);
        const void* cfg = readPtr<const void>(self, 0x18);
        const float radius = readF32(cfg, 0x40);
        const float distSq = (dz * dz + dy * dy) + dx * dx;
        if ((radius * radius) > distSq) {
            return true;
        }
    }

    if (readU8(self, 0x244) == 0u) {
        return false;
    }
    const std::uint32_t vecCount = (readU32(self, 0x1D0) - readU32(self, 0x1CC)) >> 2u;
    if (readU32(self, 0x25C) == vecCount) {
        return false;
    }
    const void* cfg = readPtr<const void>(self, 0x18);
    if ((readF32(cfg, 0x44) * readF32(self, 0x264)) > readF32(self, 0x260)) {
        return false;
    }

    const void* owner0 = readPtr<const void>(self, 0x00);
    const void* aiPlayer = readPtr<const void>(owner0, 0x90);
    const auto* baseBuilder = static_cast<const std::uint8_t*>(aiPlayer) + 0x454;
    const auto* criteriaCfg = readPtr<const AIBaseQueryCriteriaConfigView>(cfg, 0x94);
    const auto* targetPos = reinterpret_cast<const Coord3DView*>(
        static_cast<const std::uint8_t*>(self) + 0x1A4);

    const AIBaseView* bestBase = findBestMatchingBase_8908C0(
        baseBuilder, criteriaCfg, targetPos, gameLogicOverride);
    float distSqToBase = 3.4028234663852886e+38f; // [0x00C50490]
    if (bestBase != nullptr) {
        const Coord3DView* basePos = getBasePosition_890400(bestBase, gameLogicOverride);
        distSqToBase = distanceSquared3D_5049E0(targetPos, basePos);
    }

    const float minBaseDist = readF32(cfg, 0x3C);
    if ((minBaseDist * minBaseDist) > distSqToBase) {
        return false;
    }
    return readF32(cfg, 0x38) > readF32(self, 0x23C);
}

// 32. 0x00893880 — AIBuildState::findActiveObjectBySlotKey
void* findActiveObjectBySlotKey_893880(
    const void* self,
    std::int32_t slotKey,
    const void* gameLogicOverride) noexcept {
    const auto* treeBase = static_cast<const std::uint8_t*>(self) + 0x98;
    const auto* sentinel = reinterpret_cast<const RbTreeNodeView*>(
        static_cast<const std::uint8_t*>(self) + 0x9C);
    const RbTreeNodeView* node = findRbTreeNodeSigned_AAFE10(treeBase, slotKey);
    const std::uint32_t objId = (node != sentinel) ? node->valueAt14 : 0u;
    const void* gameLogic = resolveGameLogic(gameLogicOverride);
    return findObjectByIdInGameLogic(gameLogic, objId);
}

// 33. 0x00893920 — AIBuildState::getSlotAuxiliaryIdIfObjectAlive
std::uint32_t getSlotAuxiliaryIdIfObjectAlive_893920(
    const void* self,
    std::int32_t slotKey,
    const void* gameLogicOverride) noexcept {
    const auto* treeBase = static_cast<const std::uint8_t*>(self) + 0x98;
    const auto* sentinel = reinterpret_cast<const RbTreeNodeView*>(
        static_cast<const std::uint8_t*>(self) + 0x9C);
    const RbTreeNodeView* node = findRbTreeNodeSigned_AAFE10(treeBase, slotKey);
    if (node == sentinel || node->valueAt14 == 0u) {
        return 0u;
    }
    const void* gameLogic = resolveGameLogic(gameLogicOverride);
    const void* obj = findObjectByIdInGameLogic(gameLogic, node->valueAt14);
    if (obj != nullptr && (readU8(obj, 0x471) & 1u) == 0u) {
        return node->auxValueAt18;
    }
    return 0u;
}

// 34. 0x00893A00 — AIBuildState::hasAliveTrackedObjectForCandidate
bool hasAliveTrackedObjectForCandidate_893A00(
    const void* self,
    const void* candidateObject,
    const void* gameLogicOverride) noexcept {
    const std::int32_t key = readI32(candidateObject, 0xDC);
    const auto* treeBase = static_cast<const std::uint8_t*>(self) + 0x7C;
    const auto* sentinel = reinterpret_cast<const RbTreeNodeView*>(
        static_cast<const std::uint8_t*>(self) + 0x80);
    const RbTreeNodeView* node = findRbTreeNodeSigned_AAFE10(treeBase, key);
    if (node == sentinel || node->valueAt14 == 0u) {
        return false;
    }
    const void* gameLogic = resolveGameLogic(gameLogicOverride);
    const void* obj = findObjectByIdInGameLogic(gameLogic, node->valueAt14);
    return obj != nullptr && (readU8(obj, 0x471) & 1u) == 0u;
}

// 35. 0x00897BB0 — AIEconomyStatsSubobject::initDefault
void* initEconomyStatsDefault_897BB0(void* self) noexcept {
    auto* bytes = static_cast<std::uint8_t*>(self);
    writeU32(self, 0x00, kVtableAIEconomyStatsSubobject);
    writeU32(self, 0x04, 0u);
    writeU32(self, 0x08, 0u);
    writeU32(self, 0x0C, 0u);
    writeU32(self, 0x10, 0u);
    writeU32(self, 0x14, 0u);
    writePtr(self, 0x1C, bytes + 0x1C);
    writePtr(self, 0x20, bytes + 0x1C);
    writeU32(self, 0x24, 0u);
    writeU32(self, 0x28, 0u);
    writeU32(self, 0x2C, 0u);
    writeU32(self, 0x34, 0u);
    writeU32(self, 0x38, 0u);
    writeU32(self, 0x3C, 0u);
    writePtr(self, 0x48, bytes + 0x48);
    writePtr(self, 0x4C, bytes + 0x48);
    writeU32(self, 0x50, 0u);
    writeU32(self, 0x54, 0u);
    writeU32(self, 0x58, 0u);
    return self;
}

// 36. 0x00897ED0 — AIEconomyStats::updateSlidingWindow
void updateEconomyStatsSlidingWindow_897ED0(
    void* self,
    const void* gameLogicOverride) noexcept {
    auto* bytes = static_cast<std::uint8_t*>(self);
    pruneExpiredSamples_88BE90(bytes + 0x08, gameLogicOverride);
    pruneExpiredSamples_88BE90(bytes + 0x40, gameLogicOverride);
}

// 37. 0x0089A6D0 — AIBuildableEntry::resolveStructureObject
void* resolveStructureObject_89A6D0(
    const void* self,
    const void* gameLogicOverride) noexcept {
    const std::uint32_t objId = readU32(self, 0x20);
    if (objId == 0u) {
        return nullptr;
    }
    const void* gameLogic = resolveGameLogic(gameLogicOverride);
    return findObjectByIdInGameLogic(gameLogic, objId);
}

// 38. 0x0089A740 — AIBuildableEntry::getStructureOrFallbackPosition
const Coord3DView* getStructureOrFallbackPosition_89A740(
    const void* self,
    const void* gameLogicOverride) noexcept {
    const std::uint32_t objId = readU32(self, 0x20);
    if (objId != 0u) {
        const void* gameLogic = resolveGameLogic(gameLogicOverride);
        const void* obj = findObjectByIdInGameLogic(gameLogic, objId);
        if (obj != nullptr && (readU8(obj, 0x471) & 1u) == 0u) {
            return reinterpret_cast<const Coord3DView*>(
                static_cast<const std::uint8_t*>(obj) + 0x38);
        }
    }
    return reinterpret_cast<const Coord3DView*>(
        static_cast<const std::uint8_t*>(self) + 0x10);
}

// 39. 0x0089AC90 — AIBuildableStructureTracker::init
void* initBuildableStructureTracker_89AC90(
    void* self,
    const void* gameLogicOverride,
    const float* framesPerSecondOverride) noexcept {
    writeU32(self, 0x00, kVtableAIBuildableStructureTracker);
    writeU32(self, 0x08, kEmptyAsciiStringBufAddr);
    writeU32(self, 0x0C, 1u);
    writeU32(self, 0x10, 0u);
    writeF32(self, 0x14, 1.0f);
    writeF32(self, 0x18, 2.0f);
    writeU32(self, 0x1C, 0u);
    writeU32(self, 0x28, kEmptyAsciiStringBufAddr);
    writeU32(self, 0x2C, 1u);
    writeU32(self, 0x30, 0u);
    writeF32(self, 0x34, 1.0f);
    writeF32(self, 0x38, 2.0f);
    writeU32(self, 0x3C, 0u);

    const void* gameLogic = resolveGameLogic(gameLogicOverride);
    const float fps = resolveFramesPerSecond(framesPerSecondOverride);
    const double scaled = static_cast<double>(readF32(gameLogic, 0x1C4))
        * static_cast<double>(fps)
        * 5.0;
    const std::uint32_t deltaFrames = static_cast<std::uint32_t>(
        static_cast<std::int64_t>(scaled));
    writeU32(self, 0x44, readU32(gameLogic, 0x58) + deltaFrames);
    return self;
}

// 40. 0x0089AF50 — AIBuildHeuristic::initBase
void* initBuildHeuristicBase_89AF50(void* self) noexcept {
    writeU32(self, 0x00, kVtableAIBuildHeuristicBase);
    writeU32(self, 0x04, 0u);
    writeU32(self, 0x08, 0u);
    writeU32(self, 0x0C, 0u);
    writeU32(self, 0x14, 0u);
    writeU32(self, 0x18, 0u);
    writeU32(self, 0x1C, 0u);
    writeU8(self, 0x24, 0u);
    return self;
}

// 41. 0x0089D070 — AIBuilder::hasActiveBuildableEntryForObjectId
bool hasActiveBuildableEntryForObjectId_89D070(
    const void* self,
    std::uint32_t objectId,
    const void* gameLogicOverride) noexcept {
    const void* reg = readPtr<const void>(self, 0x6F4);
    const auto* base = readPtr<const std::uint8_t>(reg, 0x38);
    const std::uint32_t count = readU32(reg, 0x3C);

    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint8_t* entry = base + i * 0x2CCu;
        if (readU32(entry, 0x20) == objectId) {
            if (resolveStructureObject_89A6D0(entry, gameLogicOverride) != nullptr) {
                return true;
            }
        }
    }
    return false;
}

// 42. 0x0089D0D0 — AIBuilder::hasActiveBuildableEntryAtPosition
bool hasActiveBuildableEntryAtPosition_89D0D0(
    const void* self,
    float x,
    float y,
    float z,
    const void* gameLogicOverride) noexcept {
    const void* reg = readPtr<const void>(self, 0x6F4);
    const auto* base = readPtr<const std::uint8_t>(reg, 0x38);
    const std::uint32_t count = readU32(reg, 0x3C);

    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint8_t* entry = base + i * 0x2CCu;
        if (readU8(entry, 0x74) != 0u) {
            const Coord3DView* pos = getStructureOrFallbackPosition_89A740(
                entry, gameLogicOverride);
            if (pos->x == x && pos->y == y && pos->z == z) {
                return true;
            }
        }
    }
    return false;
}

// 43. 0x0089DD90 — AIBuildableCandidateVector::eraseRange68
void* eraseCandidateRange68_89DD90(
    void* self,
    void* eraseBegin,
    void* eraseEnd) noexcept {
    constexpr std::size_t kStride = 0x44u;
    auto* dst = static_cast<std::uint8_t*>(eraseBegin);
    auto* src = static_cast<std::uint8_t*>(eraseEnd);
    auto* vecEnd = readPtr<std::uint8_t>(self, 0x04);

    while (src != vecEnd) {
        std::memmove(dst, src, kStride);
        src += kStride;
        dst += kStride;
    }

    const std::ptrdiff_t erasedBytes = static_cast<std::uint8_t*>(eraseEnd)
        - static_cast<std::uint8_t*>(eraseBegin);
    const std::ptrdiff_t erasedCount = erasedBytes / static_cast<std::ptrdiff_t>(kStride);
    writePtr(self, 0x04, vecEnd - erasedCount * static_cast<std::ptrdiff_t>(kStride));
    return eraseBegin;
}

// 44. 0x0089E700 — AIBaseQueryCriteria::insertionSortBounded
void insertionSortBasesBounded_89E700(
    AIBaseView** first,
    AIBaseView** last,
    AIBaseQueryCriteriaView criteria,
    const void* gameLogicOverride) noexcept {
    if (first == last) {
        return;
    }
    for (AIBaseView** it = first + 1; it != last; ++it) {
        AIBaseView* val = *it;
        AIBaseView** hole = it;
        while (hole != first) {
            AIBaseView* prev = *(hole - 1);
            if (!isCandidateBetterThanBest_890600(&criteria, val, prev, gameLogicOverride)) {
                break;
            }
            *hole = *(hole - 1);
            --hole;
        }
        *hole = val;
    }
}

// 45. 0x0089E750 — AIBaseQueryCriteria::insertionSortUnguarded
void insertionSortBasesUnguarded_89E750(
    AIBaseView** first,
    AIBaseView** last,
    AIBaseQueryCriteriaView criteria,
    const void* gameLogicOverride) noexcept {
    for (AIBaseView** it = first; it != last; ++it) {
        AIBaseView* val = *it;
        AIBaseView** hole = it;
        AIBaseView** prevPtr = it - 1;
        if (isCandidateBetterThanBest_890600(&criteria, val, *prevPtr, gameLogicOverride)) {
            do {
                *hole = *prevPtr;
                hole = prevPtr;
                --prevPtr;
            } while (isCandidateBetterThanBest_890600(&criteria, val, *prevPtr, gameLogicOverride));
        }
        *hole = val;
    }
}

// 46. 0x0089F140 — AIBaseQueryCriteria::selectMedianOfThree
AIBaseView** selectMedianOfThreeBases_89F140(
    AIBaseView** first,
    AIBaseView** mid,
    AIBaseView** last,
    AIBaseQueryCriteriaView criteria,
    const void* gameLogicOverride) noexcept {
    if (isCandidateBetterThanBest_890600(&criteria, *first, *mid, gameLogicOverride)) {
        if (isCandidateBetterThanBest_890600(&criteria, *mid, *last, gameLogicOverride)) {
            return mid;
        }
        if (isCandidateBetterThanBest_890600(&criteria, *first, *last, gameLogicOverride)) {
            return last;
        }
        return first;
    }
    if (isCandidateBetterThanBest_890600(&criteria, *first, *last, gameLogicOverride)) {
        return first;
    }
    if (isCandidateBetterThanBest_890600(&criteria, *mid, *last, gameLogicOverride)) {
        return last;
    }
    return mid;
}

// 47. 0x0089F1D0 — AIBaseQueryCriteria::partitionByPivot
AIBaseView** partitionBasesByPivot_89F1D0(
    AIBaseView** first,
    AIBaseView** last,
    AIBaseView** pivotPtr,
    AIBaseQueryCriteriaView criteria,
    const void* gameLogicOverride) noexcept {
    AIBaseView* pivot = *pivotPtr;
    AIBaseView** lo = first;
    AIBaseView** hi = last;
    for (;;) {
        while (isCandidateBetterThanBest_890600(&criteria, *lo, pivot, gameLogicOverride)) {
            ++lo;
        }
        --hi;
        while (isCandidateBetterThanBest_890600(&criteria, pivot, *hi, gameLogicOverride)) {
            --hi;
        }
        if (lo >= hi) {
            return lo;
        }
        AIBaseView* tmp = *lo;
        *lo = *hi;
        *hi = tmp;
        ++lo;
    }
}

// 48. 0x0089F480 — AIBuildHeuristicStore::init
void* initBuildHeuristicStore_89F480(void* self) noexcept {
    auto* bytes = static_cast<std::uint8_t*>(self);
    writeU32(self, 0x000, kVtableAIBuildHeuristicStore);

    auto initHeuristicSub = [&](std::ptrdiff_t baseOff, std::uint32_t vft) noexcept {
        writeU32(self, baseOff + 0x00, vft);
        writePtr(self, baseOff + 0x08, bytes + baseOff + 0x08);
        writePtr(self, baseOff + 0x0C, bytes + baseOff + 0x08);
        writeU32(self, baseOff + 0x10, 0u);
        writeU32(self, baseOff + 0x14, 0u);
        writeU32(self, baseOff + 0x18, 0u);
    };

    initHeuristicSub(0x004, 0x00C51F7Cu);
    initHeuristicSub(0x024, 0x00C51F94u);
    initHeuristicSub(0x044, 0x00C51F88u);
    initHeuristicSub(0x064, 0x00C51FA0u);
    initHeuristicSub(0x084, 0x00C51FACu);
    initHeuristicSub(0x0A4, 0x00C51FB8u);
    initHeuristicSub(0x0C8, 0x00C51FC4u);
    initHeuristicSub(0x0E8, 0x00C51FE0u);
    writeU32(self, 0x108, 0x00C51FD0u);
    writeU32(self, 0x10C, 0u);
    writeU32(self, 0x110, 0u);
    writeU32(self, 0x114, 0u);
    initHeuristicSub(0x11C, 0x00C52004u);
    initHeuristicSub(0x13C, 0x00C52010u);
    initHeuristicSub(0x15C, 0x00C5201Cu);
    initHeuristicSub(0x17C, 0x00C52028u);
    initHeuristicSub(0x19C, 0x00C52034u);
    initHeuristicSub(0x1BC, 0x00C52040u);
    initHeuristicSub(0x1DC, 0x00C5204Cu);
    initHeuristicSub(0x1FC, 0x00C52054u);
    initHeuristicSub(0x21C, 0x00C52320u);
    initHeuristicSub(0x23C, 0x00C5232Cu);
    return self;
}

// 49. 0x0089FE10 — AIBaseBuilder::pruneAssignedStructures
void pruneAssignedStructures_89FE10(void* self) noexcept {
    auto* activeBegin = readPtr<std::uint32_t>(self, 0x5C);
    auto* activeEnd = readPtr<std::uint32_t>(self, 0x60);
    if (activeBegin == activeEnd) {
        return;
    }

    std::uint32_t* cur = activeBegin;
    while (cur != readPtr<std::uint32_t>(self, 0x60)) {
        const std::uint32_t structId = *cur;
        auto** basesBegin = readPtr<AIBaseView*>(self, 0x28);
        auto** basesEnd = readPtr<AIBaseView*>(self, 0x2C);
        bool foundInBase = false;

        for (AIBaseView** bIt = basesBegin; bIt != basesEnd; ++bIt) {
            const AIBaseView* base = *bIt;
            std::uint32_t* sIt = base->structuresBeginAt30;
            std::uint32_t* sEnd = base->structuresEndAt34;
            while (sIt != sEnd) {
                if (*sIt == structId) {
                    break;
                }
                ++sIt;
            }
            if (sIt != sEnd) {
                auto* oldEnd = readPtr<std::uint32_t>(self, 0x60);
                const std::size_t tailBytes = static_cast<std::size_t>(
                    reinterpret_cast<std::uint8_t*>(oldEnd)
                    - reinterpret_cast<std::uint8_t*>(cur + 1));
                if (tailBytes != 0u) {
                    std::memmove(cur, cur + 1, tailBytes);
                }
                writePtr(self, 0x60, oldEnd - 1);
                foundInBase = true;
                break;
            }
        }

        if (!foundInBase) {
            ++cur;
        }
    }
}

// 50. 0x0089FEB0 — AIBuildStateManager::removeMatchingOrProducedTemplate
void removeMatchingOrProducedTemplate_89FEB0(
    void* self,
    std::uint32_t templateId,
    const void* thingFactoryOverride) noexcept {
    const void* factory = resolveThingFactory(thingFactoryOverride);
    const void* targetTmpl = findTemplateById_78D880(factory, templateId);
    auto* begin = readPtr<std::uint32_t>(self, 0x24);
    auto* end = readPtr<std::uint32_t>(self, 0x28);
    if (begin == end) {
        return;
    }

    const std::uint32_t targetIdAt8 = readU32(targetTmpl, 0x08);
    for (std::uint32_t* it = begin; it != end; ++it) {
        if (*it == targetIdAt8) {
            auto* curEnd = readPtr<std::uint32_t>(self, 0x28);
            const std::size_t tailCount = static_cast<std::size_t>(curEnd - (it + 1));
            for (std::size_t k = 0; k < tailCount; ++k) {
                it[k] = it[k + 1];
            }
            writePtr(self, 0x28, curEnd - 1);
            return;
        }
        const void* entryTmpl = findTemplateById_78D880(factory, *it);
        if (entryTmpl != nullptr) {
            if (isTemplateEquivalent_78DA40(entryTmpl, targetTmpl, factory)) {
                auto* curEnd = readPtr<std::uint32_t>(self, 0x28);
                const std::size_t tailCount = static_cast<std::size_t>(curEnd - (it + 1));
                for (std::size_t k = 0; k < tailCount; ++k) {
                    it[k] = it[k + 1];
                }
                writePtr(self, 0x28, curEnd - 1);
                return;
            }
        }
        end = readPtr<std::uint32_t>(self, 0x28);
    }
}

// 51. 0x008AAB70 — AIBuildableStructure::getEffectivePriority
float getEffectivePriority_8AAB70(const void* self) noexcept {
    const float basePriority = readF32(self, 0x08);
    const float overridePriority = readF32(self, 0x24);
    if (overridePriority > basePriority && basePriority != -1.0f) {
        return overridePriority;
    }
    return basePriority;
}

// 52. 0x008AABB0 — AIBuildableStructure::getValuePerCost
float getStructureValuePerCost_8AABB0(void* self, void* aiContext) noexcept {
    const auto* vft = readPtr<const std::uintptr_t>(self, 0x00);
    using EvalFn = float(__fastcall*)(void*, void*, void*);
    const auto getValue = reinterpret_cast<EvalFn>(vft[13]); // +0x34
    const auto getCost = reinterpret_cast<EvalFn>(vft[15]);  // +0x3C
    const double val = static_cast<double>(getValue(self, nullptr, aiContext));
    const double cost = static_cast<double>(getCost(self, nullptr, aiContext));
    return static_cast<float>(val / cost);
}

// 53. 0x008AABE0 — AIBuildableUnit::getValuePerCost
float getUnitValuePerCost_8AABE0(void* self, void* aiContext) noexcept {
    const auto* vft = readPtr<const std::uintptr_t>(self, 0x00);
    using EvalFn = float(__fastcall*)(void*, void*, void*);
    const auto getValue = reinterpret_cast<EvalFn>(vft[13]); // +0x34
    const auto getCost = reinterpret_cast<EvalFn>(vft[14]);  // +0x38
    const double val = static_cast<double>(getValue(self, nullptr, aiContext));
    const double cost = static_cast<double>(getCost(self, nullptr, aiContext));
    return static_cast<float>(val / cost);
}

// 54. 0x008AAC20 — AIRebuildableStructure::getScaledDelayFrames
float getScaledDelayFrames_8AAC20(
    const void* /*self*/,
    std::uint32_t /*unusedArg*/,
    const void* gameLogicOverride,
    const float* framesPerSecondOverride) noexcept {
    const void* gameLogic = resolveGameLogic(gameLogicOverride);
    const float fps = resolveFramesPerSecond(framesPerSecondOverride);

    double prod = static_cast<double>(fps);
    if (gameLogic != nullptr) {
        prod = static_cast<double>(readF32(gameLogic, 0x1C4)) * static_cast<double>(fps);
    }
    const std::int64_t trunc64 = static_cast<std::int64_t>(prod);
    const std::uint32_t trunc32 = static_cast<std::uint32_t>(trunc64);
    double asFloat = static_cast<double>(static_cast<std::int32_t>(trunc32));
    if (static_cast<std::int32_t>(trunc32) < 0) {
        asFloat += 4294967296.0; // [0x00BDBDD4]
    }
    return static_cast<float>(asFloat);
}

// 55. 0x008AEEB0 — AIBaseBuilder::moveStructureToUnderConstruction
void moveStructureToUnderConstruction_8AEEB0(
    void* self,
    std::uint32_t objectId) noexcept {
    auto* activeBegin = readPtr<std::uint32_t>(self, 0x5C);
    auto* activeEnd = readPtr<std::uint32_t>(self, 0x60);
    for (std::uint32_t* it = activeBegin; it != activeEnd; ++it) {
        if (*it == objectId) {
            return;
        }
    }

    insertU32AtEndInPlace_8ADD50(
        static_cast<std::uint8_t*>(self) + 0x5C, &objectId);

    auto* completedBegin = readPtr<std::uint32_t>(self, 0x6C);
    auto* completedEnd = readPtr<std::uint32_t>(self, 0x70);
    for (std::uint32_t* it = completedBegin; it != completedEnd; ++it) {
        if (*it == objectId) {
            const std::size_t tailBytes = static_cast<std::size_t>(
                reinterpret_cast<std::uint8_t*>(completedEnd)
                - reinterpret_cast<std::uint8_t*>(it + 1));
            if (tailBytes != 0u) {
                std::memmove(it, it + 1, tailBytes);
            }
            writePtr(self, 0x70, completedEnd - 1);
            return;
        }
    }
}

// 56. 0x008AEF30 — AIBaseBuilder::moveStructureToCompleted
void moveStructureToCompleted_8AEF30(
    void* self,
    std::uint32_t objectId) noexcept {
    auto* completedBegin = readPtr<std::uint32_t>(self, 0x6C);
    auto* completedEnd = readPtr<std::uint32_t>(self, 0x70);
    for (std::uint32_t* it = completedBegin; it != completedEnd; ++it) {
        if (*it == objectId) {
            return;
        }
    }

    insertU32AtEndInPlace_8ADD50(
        static_cast<std::uint8_t*>(self) + 0x6C, &objectId);

    auto* activeBegin = readPtr<std::uint32_t>(self, 0x5C);
    auto* activeEnd = readPtr<std::uint32_t>(self, 0x60);
    for (std::uint32_t* it = activeBegin; it != activeEnd; ++it) {
        if (*it == objectId) {
            const std::size_t tailBytes = static_cast<std::size_t>(
                reinterpret_cast<std::uint8_t*>(activeEnd)
                - reinterpret_cast<std::uint8_t*>(it + 1));
            if (tailBytes != 0u) {
                std::memmove(it, it + 1, tailBytes);
            }
            writePtr(self, 0x60, activeEnd - 1);
            return;
        }
    }
}

} // namespace ra3::gamelogic::skirmishai_base_build
