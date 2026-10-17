#include "ra3/gamelogic/SkirmishAI87.h"

#include <ra3/sim/Random.h>

#include <cstddef>
#include <cstring>

namespace ra3::gamelogic {
namespace {

template <typename T>
T load(const void* base, std::ptrdiff_t offset) noexcept {
    T value{};
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, sizeof(T));
    return value;
}

template <typename T>
void store(void* base, std::ptrdiff_t offset, T value) noexcept {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(T));
}

std::uint8_t* pointerAt(const void* base, std::ptrdiff_t offset) noexcept {
    return reinterpret_cast<std::uint8_t*>(
        static_cast<std::uintptr_t>(load<std::uint32_t>(base, offset)));
}

void copyFloatTriple(const std::uint8_t* source, std::uint8_t* destination) noexcept {
    std::memcpy(destination, source, 12);
}

void zeroFloatTriple(std::uint8_t* destination) noexcept {
    const std::uint32_t zero = 0;
    store<std::uint32_t>(destination, 0, zero);
    store<std::uint32_t>(destination, 4, zero);
    store<std::uint32_t>(destination, 8, zero);
}

std::uint8_t versionByte(XferOps& ops, std::uint8_t current, std::uint8_t supported) noexcept {
    std::uint8_t pair[2] = {current, supported};
    ops.version(ops.xfer, pair);
    return pair[1];
}

void xferObjectId(XferOps& ops, void* field) noexcept {
    ops.field88(ops.xfer, field, 0x00BE1D24u, 4u);
}

void xferProductionQueue(XferOps& ops, void* field) noexcept {
    ops.field88(ops.xfer, field, 0x00BE1D10u, 4u);
}

constexpr std::uint32_t kBankAccountName = 0x00C50584u;
constexpr std::uint32_t kAttackStateName = 0x00C5180Cu;

}  // namespace

std::uint32_t applyStaticDefenseTacticRecord(std::uint8_t* target) noexcept {
    store<std::uint32_t>(target, 0x210, 1u);
    store<std::uint32_t>(target, 0x21C, 0x400u);
    for (int index = 1; index < 9; ++index) {
        store<std::uint32_t>(target, 0x21C + index * 4, 0u);
    }
    return 0x400u;
}

std::uint32_t applyHuntTacticFlags(std::uint8_t* target) noexcept {
    store<std::uint32_t>(target, 0x214, 1u);
    store<std::uint32_t>(target, 0x160, 0u);
    store<std::uint32_t>(target, 0x294, load<std::uint32_t>(target, 0x294) | 0x10u);
    target[0x2B2] = 1;
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(target));
}

std::uint32_t applySimpleDefenseTacticMode(std::uint8_t* target) noexcept {
    store<std::uint32_t>(target, 0x210, 5u);
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(target));
}

std::uint32_t applySimpleSiegeTacticFlag(std::uint8_t* target) noexcept {
    store<std::uint32_t>(target, 0x21C, load<std::uint32_t>(target, 0x21C) | 0x02000000u);
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(target));
}

std::uint32_t clearSimpleDefenseAlertByte(std::uint8_t* self) noexcept {
    auto* nested = pointerAt(self, 0x2C);
    nested[0x10A] = 0;
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(nested));
}

std::uint32_t tacticArgumentPresent(std::uint32_t argument) noexcept {
    return argument != 0u ? 1u : 0u;
}

std::uint32_t copyRebuildableStructurePosition(std::uint8_t* self, std::uint8_t* destination) noexcept {
    auto* record = pointerAt(self, 0x70);
    auto* position = record != nullptr ? pointerAt(record, 0x0C) : nullptr;
    if (position != nullptr) {
        copyFloatTriple(position, destination);
    } else {
        zeroFloatTriple(destination);
    }
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(destination));
}

std::uint32_t copyBuildableStructureAnchor(std::uint8_t* self, std::uint8_t* destination) noexcept {
    copyFloatTriple(pointerAt(self, 0x44), destination);
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(destination));
}

std::uint32_t rebuildableStructurePositionPointer(const std::uint8_t* self) noexcept {
    auto* record = pointerAt(self, 0x70);
    if (record == nullptr) {
        return 0u;
    }
    return load<std::uint32_t>(record, 0x0C);
}

std::uint32_t rebuildableStructureRadiusPointer(const std::uint8_t* self) noexcept {
    auto* record = pointerAt(self, 0x70);
    if (record == nullptr) {
        return 0u;
    }
    return load<std::uint32_t>(record, 0x10);
}

RadiusResult rebuildableStructureRadiusOrZero(const std::uint8_t* self) noexcept {
    RadiusResult result{};
    auto* record = pointerAt(self, 0x70);
    if (record != nullptr) {
        result.pointer = load<std::uint32_t>(record, 0x10);
        if (result.pointer != 0u) {
            result.floatBits = load<std::uint32_t>(
                reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(result.pointer)), 0);
            return result;
        }
    }
    result.pointer = 0u;
    result.floatBits = 0u;
    return result;
}

void copyBuildableStructureScore(std::uint8_t* self) noexcept {
    store<std::uint32_t>(self, 0x24, load<std::uint32_t>(self, 8));
}

std::uint32_t aiRedAlertFrameReached(const std::uint8_t* self, std::uint32_t frame) noexcept {
    const auto due = load<std::uint32_t>(self, 0x14);
    return frame < due ? 0u : 1u;
}

std::uint32_t armAIRedAlertFrame(std::uint8_t* self, std::uint32_t gameLogic,
                                 std::uint32_t frame) noexcept {
    store<std::uint32_t>(self, 0x14, frame + load<std::uint32_t>(self, 0x10));
    return gameLogic;
}

std::uint32_t attackObjectPresent(const std::uint8_t* self) noexcept {
    return load<std::uint32_t>(self, 0x28) != 0u ? 1u : 0u;
}

std::uint32_t rollAITacticActivation(std::uint8_t* target, RandomWords& logic) noexcept {
    ra3::sim::RandomState state{};
    for (int index = 0; index < 6; ++index) {
        state.words[static_cast<std::size_t>(index)] = logic.word[index];
    }
    const int rolled = ra3::sim::logicRandomInteger(state, 3, 5);
    for (int index = 0; index < 6; ++index) {
        logic.word[index] = state.words[static_cast<std::size_t>(index)];
    }
    const auto value = static_cast<std::uint32_t>(rolled);
    store<std::uint32_t>(target, 0x214, value);
    return value;
}

std::uint8_t* treeSuccessor(std::uint8_t* node) noexcept {
    auto* right = pointerAt(node, 0);
    if (right != nullptr) {
        node = right;
        auto* left = pointerAt(node, 4);
        while (left != nullptr) {
            node = left;
            left = pointerAt(node, 4);
        }
        return node;
    }
    auto* parent = pointerAt(node, 8);
    while (node == pointerAt(parent, 0)) {
        node = parent;
        parent = pointerAt(node, 8);
    }
    if (pointerAt(node, 0) != parent) {
        return parent;
    }
    return node;
}

std::uint32_t findSimpleExpansionRecord(std::uint8_t* header, std::uint32_t key) noexcept {
    auto* node = pointerAt(header, 0x18);
    auto* sentinel = header + 0x14;
    if (node == sentinel) {
        return 0u;
    }
    for (;;) {
        auto* first = pointerAt(node, 0x14);
        auto* second = pointerAt(first, 0x0C);
        auto* third = pointerAt(second, 4);
        auto* keySlot = pointerAt(third, 0x18);
        if (load<std::uint32_t>(keySlot, 0) == key) {
            return load<std::uint32_t>(node, 0x10);
        }
        node = treeSuccessor(node);
        if (node == sentinel) {
            return 0u;
        }
    }
}

std::uint32_t cacheSimpleExpansionRecord(std::uint8_t* self) noexcept {
    const auto found = findSimpleExpansionRecord(self + 0x58, 2u);
    store<std::uint32_t>(self, 0xA0, found);
    return found;
}

std::uint32_t ensureBuildablePhaseFour(std::uint8_t* self, PhaseSlot invoke) noexcept {
    if (load<std::uint32_t>(self, 0x14) == 4u) {
        return 0u;
    }
    return invoke(self, 4u);
}

void xferPrerequisiteBuilder(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    ops.field20(ops.xfer, self + 8);
}

void xferUnitBuilder(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    ops.field20(ops.xfer, self + 8);
    ops.field6c(ops.xfer, self + 0x18);
    ops.field64(ops.xfer, self + 0x20);
}

void xferBuildableStructureFactory(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    ops.field84(ops.xfer, self + 4);
    ops.field20(ops.xfer, self + 8);
}

void xferEmergencyManager(std::uint8_t* self, XferOps& ops) noexcept {
    const auto fileVersion = versionByte(ops, 1, 2);
    ops.field20(ops.xfer, self + 8);
    ops.field84(ops.xfer, self + 0x1C);
    ops.field84(ops.xfer, self + 0x1D);
    if (fileVersion >= 2) {
        ops.field6c(ops.xfer, self + 0x20);
    }
}

void xferSubsystemEnabler(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    ops.field84(ops.xfer, self + 4);
    ops.field6c(ops.xfer, self + 8);
    ops.field6c(ops.xfer, self + 0x0C);
}

void xferEconomyManager(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    ops.field20(ops.xfer, self + 0x60);
}

void xferRedAlertManager(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    ops.field20(ops.xfer, self + 4);
    ops.field6c(ops.xfer, self + 0x14);
}

void xferAIBank(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    auto* cursor = self + 8;
    for (int remaining = 6; remaining != 0; --remaining) {
        ops.field20(ops.xfer, cursor);
        cursor += 0x24;
    }
    ops.field20(ops.xfer, self + 0xE0);
    const auto flags = load<std::uint32_t>(ops.xfer, 4);
    if (((flags >> 1) & 1u) != 0u) {
        store<std::uint32_t>(self, 0xFC, 0u);
    }
}

void xferBuildHeuristicStore(std::uint8_t* self, XferOps& ops) noexcept {
    const auto fileVersion = versionByte(ops, 1, 2);
    if (fileVersion < 2) {
        return;
    }
    // lea eax, [edi+0xE8] is never null, so the empty-pointer arm is unreachable.
    ops.field20(ops.xfer, self + 0x108);
}

void xferSharedBuildRecord(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    ops.field64(ops.xfer, self + 8);
    xferObjectId(ops, self + 0x0C);
    ops.field6c(ops.xfer, self + 0x10);
    auto saved = load<std::uint32_t>(self, 0x14);
    ops.field6c(ops.xfer, &saved);
    store<std::uint32_t>(self, 0x14, saved);
    ops.field64(ops.xfer, self + 0x24);
    ops.field84(ops.xfer, self + 0x28);
    xferObjectId(ops, self + 0x30);
    ops.field84(ops.xfer, self + 0x34);
    xferProductionQueue(ops, self + 0x18);
    ops.field84(ops.xfer, self + 0x29);
    ops.field84(ops.xfer, self + 0x2A);
    ops.field84(ops.xfer, self + 0x2B);
    ops.field6c(ops.xfer, self + 0x38);
    ops.field88(ops.xfer, self + 0x1C, kBankAccountName, 4u);
    ops.field6c(ops.xfer, self + 0x2C);
}

void xferBuildableUnit(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    xferSharedBuildRecord(self, ops);
    ops.field58(ops.xfer, self + 0x40);
    ops.field70(ops.xfer, self + 0x4C);
}

void xferAIUpgrade(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    xferSharedBuildRecord(self, ops);
    ops.field70(ops.xfer, self + 0x40);
    auto* outer = pointerAt(self, 0x44);
    if (outer != nullptr) {
        auto* middle = pointerAt(outer, 4);
        auto* inner = pointerAt(middle, 0);
        store<std::uint32_t>(self, 0x48, load<std::uint32_t>(inner, 4));
    }
    ops.field6c(ops.xfer, self + 0x48);
}

void xferUpgradeOpeningMove(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    ops.field84(ops.xfer, self + 0x4C);
    xferAIUpgrade(self, ops);
}

void xferSpecialPowerAttackObject(std::uint8_t* self, XferOps& ops) noexcept {
    const auto fileVersion = versionByte(ops, 1, 3);
    if (fileVersion >= 2) {
        ops.field6c(ops.xfer, self + 0x14);
        ops.field70(ops.xfer, self + 0x18);
    }
    ops.field88(ops.xfer, self + 0x1C, kAttackStateName, 4u);
    xferObjectId(ops, self + 0x20);
    xferObjectId(ops, self + 0x24);
    ops.field6c(ops.xfer, self + 0x28);
    ops.field6c(ops.xfer, self + 0x2C);
    if (fileVersion >= 3) {
        ops.field6c(ops.xfer, self + 0x30);
    }
}

void xferStrategyManager(std::uint8_t* self, XferOps& ops) noexcept {
    versionByte(ops, 1, 1);
    ops.field20(ops.xfer, self + 0x18);
    ops.field20(ops.xfer, self + 0x74);
    // lea of self+0xA0 is never null, so the transferred address is self+0xA4.
    ops.field20(ops.xfer, self + 0xA4);
    ops.field20(ops.xfer, self + 8);
}

}  // namespace ra3::gamelogic
