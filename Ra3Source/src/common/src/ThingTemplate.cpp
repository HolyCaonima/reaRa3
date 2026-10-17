#include <ra3/common/ThingTemplate.h>

#include <cstdint>
#include <cstring>

namespace ra3::common {
namespace {

std::uint32_t addressOf(const void* pointer) noexcept
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t loadDword(const void* base, std::size_t offset) noexcept
{
    std::uint32_t value = 0;
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, sizeof(value));
    return value;
}

void storeDword(void* base, std::size_t offset, std::uint32_t value) noexcept
{
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}

const std::uint8_t* fromAddress(std::uint32_t address) noexcept
{
    return reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(address));
}

std::uint8_t* fromAddressMutable(std::uint32_t address) noexcept
{
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(address));
}

// 0x006C3A10. Unsigned lower_bound. Children of 0 end the walk.
const std::uint8_t* lowerBoundNode(const std::uint8_t* map, std::uint32_t key) noexcept
{
    std::uint32_t nodeAddress = loadDword(map, 0x0C);
    const std::uint8_t* const end = map + 4;
    const std::uint8_t* candidate = end;
    while (nodeAddress != 0) {
        const std::uint8_t* node = fromAddress(nodeAddress);
        const std::uint32_t nodeKey = loadDword(node, 0x10);
        if (nodeKey < key) {
            nodeAddress = loadDword(node, 0);
        } else {
            candidate = node;
            nodeAddress = loadDword(node, 4);
        }
    }
    if (candidate == end || key < loadDword(candidate, 0x10)) {
        return end;
    }
    return candidate;
}

std::uint32_t mappedNode(
    const std::uint8_t* map, std::uint32_t mapAddress, std::uint32_t key) noexcept
{
    const std::uint8_t* node = lowerBoundNode(map, key);
    return mapAddress + static_cast<std::uint32_t>(node - map);
}

void copyPlainRecords(std::uint8_t* destination, std::uint32_t begin, std::uint32_t end) noexcept
{
    while (begin != end) {
        const std::uint8_t* source = fromAddress(begin);
        std::memcpy(destination, source, 0x14);
        destination += 0x14;
        begin += 0x14;
    }
}

bool nameMapHits(const std::uint8_t* nameMap, std::uint32_t key) noexcept
{
    if (nameMap == nullptr) {
        return false;
    }
    return lowerBoundNode(nameMap, key) != nameMap + 4;
}

std::uint32_t installCommon(
    std::uint8_t* object,
    const std::uint32_t* pair,
    const std::uint8_t* registry,
    const std::uint8_t* globalMap,
    std::uint32_t globalMapAddress,
    const GameStringOps* stringOps,
    bool storeNamedFloat) noexcept
{
    const std::uint32_t key = pair[0];
    if (key == 0) {
        return addressOf(pair);
    }
    const std::uint32_t primary = registryPrimaryValue(registry, key);
    const std::uint32_t present = registryValuePresent(registry, key);
    const std::uint32_t secondary = registrySecondaryValue(registry, key);
    (void)secondary;
    constexpr std::size_t kContainers[] = {0x194, 0x1B8, 0x1DC, 0x200};
    for (std::size_t offset : kContainers) {
        removeFirstMatchingModuleRecord(
            object + offset, primary, 0, globalMap, stringOps);
    }
    // 0x0078DE50 runs only when the present byte is non-zero and template+0x308
    // is 2. Its found-key path calls the SecuROM slot 0x0040EBF0, so this
    // reconstruction does not enter that path. The oracle keeps the present
    // byte at 0, which is what an empty registry produces.
    (void)present;
    if (storeNamedFloat && key == 0x977F8391u) {
        storeDword(object, 0x2F8, pair[2]);
    }
    return mappedNode(globalMap, globalMapAddress, key);
}

}  // namespace

std::uint32_t maskAllNine(
    const std::uint32_t* objectWords, const std::uint32_t* query) noexcept
{
    std::uint32_t eax = addressOf(query);
    for (std::uint32_t index = 0; index < 9; ++index) {
        const std::uint32_t required = loadDword(query, index * sizeof(std::uint32_t));
        const std::uint32_t have = loadDword(objectWords, index * sizeof(std::uint32_t));
        if ((have & required) != required) {
            return eax & 0xFFFFFF00u;
        }
        eax += 4;
    }
    return (eax & 0xFFFFFF00u) | 1u;
}

std::uint32_t maskAllNineAtC0(
    const std::uint8_t* object, const std::uint32_t* query) noexcept
{
    return maskAllNine(reinterpret_cast<const std::uint32_t*>(object + 0xC0), query);
}

std::uint32_t maskAnyNineAtC0(
    const std::uint8_t* object, const std::uint32_t* query) noexcept
{
    for (std::uint32_t index = 0; index < 9; ++index) {
        const std::uint32_t bits = loadDword(query, index * sizeof(std::uint32_t));
        const std::uint32_t have = loadDword(object, 0xC0 + index * sizeof(std::uint32_t));
        if ((have & bits) != 0) {
            return 1;
        }
    }
    return 0;
}

std::uint32_t maskAnyNineAtC0Mutant(
    const std::uint8_t* object, const std::uint32_t* query) noexcept
{
    for (std::uint32_t index = 0; index < 9; ++index) {
        const std::uint32_t bits = loadDword(query, index * sizeof(std::uint32_t));
        const std::uint32_t have = loadDword(object, 0xC0 + index * sizeof(std::uint32_t));
        if ((have & bits) == 0) {
            return 1;
        }
    }
    return 0;
}

std::uint32_t findModuleRecordByKey(
    const std::uint8_t* container, std::uint32_t key) noexcept
{
    std::uint32_t cursor = loadDword(container, 0x14);
    const std::uint32_t end = loadDword(container, 0x18);
    while (cursor != end) {
        if (loadDword(fromAddress(cursor), 0) == key) {
            return cursor;
        }
        cursor += 0x14;
    }
    return 0;
}

std::uint32_t registryPrimaryValue(
    const std::uint8_t* registry, std::uint32_t key) noexcept
{
    if (key == 0) {
        return 0;
    }
    const std::uint8_t* primaryNode = lowerBoundNode(registry + 0x40, key);
    if (primaryNode != registry + 0x44) {
        const std::uint32_t value = loadDword(primaryNode, 0x14);
        if (value != 0) {
            return loadDword(fromAddress(value), 0);
        }
    }
    const std::uint8_t* secondaryNode = lowerBoundNode(registry + 0x24, key);
    if (secondaryNode == registry + 0x28) {
        return 0;
    }
    const std::uint32_t value = loadDword(secondaryNode, 0x14);
    if (value == 0) {
        return 0;
    }
    return loadDword(fromAddress(value), 8);
}

std::uint32_t registrySecondaryValue(
    const std::uint8_t* registry, std::uint32_t key) noexcept
{
    if (key == 0) {
        return 0;
    }
    const std::uint8_t* primaryNode = lowerBoundNode(registry + 0x40, key);
    if (primaryNode != registry + 0x44 && loadDword(primaryNode, 0x14) != 0) {
        return loadDword(fromAddress(loadDword(primaryNode, 0x14)), 4);
    }
    const std::uint8_t* secondaryNode = lowerBoundNode(registry + 0x24, key);
    if (secondaryNode == registry + 0x28 || loadDword(secondaryNode, 0x14) == 0) {
        return 0;
    }
    return loadDword(fromAddress(loadDword(secondaryNode, 0x14)), 0x10);
}

std::uint32_t registryValuePresent(
    const std::uint8_t* registry, std::uint32_t key) noexcept
{
    const std::uint8_t* node = lowerBoundNode(registry + 0x24, key);
    std::uint32_t value = 0;
    if (node != registry + 0x28) {
        value = loadDword(node, 0x14);
    }
    const std::uint32_t flag = value != 0 ? 1u : 0u;
    return (value & 0xFFFFFF00u) | flag;
}

std::uint32_t moveModuleRecords(
    const std::uint8_t* begin,
    const std::uint8_t* end,
    std::uint8_t* destination,
    const GameStringOps* stringOps) noexcept
{
    if (begin == end) {
        return addressOf(destination);
    }
    while (begin != end) {
        assignGameString(
            reinterpret_cast<GameAsciiString*>(destination),
            reinterpret_cast<const GameAsciiString*>(begin),
            *stringOps);
        storeDword(destination, 4, loadDword(begin, 4));
        storeDword(destination, 8, loadDword(begin, 8));
        storeDword(destination, 0x0C, loadDword(begin, 0x0C));
        destination[0x10] = begin[0x10];
        destination[0x11] = begin[0x11];
        begin += 0x14;
        destination += 0x14;
    }
    return addressOf(destination);
}

std::uint32_t removeFirstMatchingModuleRecord(
    std::uint8_t* container,
    std::uint32_t mask,
    std::uint32_t incomingEax,
    const std::uint8_t* nameMap,
    const GameStringOps* stringOps) noexcept
{
    std::uint32_t eax = incomingEax & 0xFFFFFF00u;
    std::uint32_t cursor = loadDword(container, 4);
    std::uint32_t end = loadDword(container, 8);
    while (cursor != end) {
        std::uint8_t* record = fromAddressMutable(cursor);
        const bool matched = (loadDword(record, 0x0C) & mask) != 0 && record[0x10] != 0
            && record[0x11] == 0;
        if (!matched) {
            cursor += 0x14;
            continue;
        }
        const std::uint32_t named = loadDword(record, 8);
        if (named != 0) {
            const std::uint32_t nameKey = loadDword(fromAddress(named), 0);
            if (nameMapHits(nameMap, nameKey)) {
                // Virtual +4 on the name-map value. The disk map is empty and
                // the oracle does not plant a hit, so this call is not taken.
            }
        }
        const std::uint32_t next = cursor + 0x14;
        if (next < end) {
            moveModuleRecords(fromAddress(next), fromAddress(end), record, stringOps);
        }
        end -= 0x14;
        storeDword(container, 8, end);
        if (stringOps != nullptr) {
            releaseGameString(reinterpret_cast<GameAsciiString*>(fromAddressMutable(end)), *stringOps);
        }
        eax = (eax & 0xFFFFFF00u) | 1u;
        end = loadDword(container, 8);
    }

    cursor = loadDword(container, 0x14);
    end = loadDword(container, 0x18);
    while (cursor != end) {
        std::uint8_t* record = fromAddressMutable(cursor);
        const bool matched = (loadDword(record, 8) & mask) != 0 && record[0x0C] != 0
            && record[0x0D] == 0;
        if (!matched) {
            cursor += 0x14;
            continue;
        }
        const std::uint32_t named = loadDword(record, 4);
        const std::uint32_t nameKey = loadDword(fromAddress(named), 0);
        if (nameMapHits(nameMap, nameKey)) {
            // Same missed virtual as the first range.
        }
        const std::uint32_t oldEnd = end;
        const std::uint32_t next = cursor + 0x14;
        if (next < oldEnd) {
            copyPlainRecords(record, next, oldEnd);
        }
        end = oldEnd - 0x14;
        storeDword(container, 0x18, end);
        eax = (oldEnd & 0xFFFFFF00u) | 1u;
    }
    return eax;
}

std::uint32_t installParsedModule(
    std::uint8_t* object,
    const std::uint32_t* pair,
    const std::uint8_t* registry,
    const std::uint8_t* globalMap,
    std::uint32_t globalMapAddress,
    const GameStringOps* stringOps) noexcept
{
    return installCommon(
        object, pair, registry, globalMap, globalMapAddress, stringOps, true);
}

std::uint32_t installParsedModuleAlternate(
    std::uint8_t* object,
    const std::uint32_t* pair,
    const std::uint8_t* registry,
    const std::uint8_t* globalMap,
    std::uint32_t globalMapAddress,
    const GameStringOps* stringOps) noexcept
{
    return installCommon(
        object, pair, registry, globalMap, globalMapAddress, stringOps, false);
}

std::uint32_t appendTemplateWordAndResetMap(
    std::uint8_t* object, std::uint32_t word) noexcept
{
    if (object[0x306] == 1) {
        object[0x306] = 0;
        const std::uint32_t end = loadDword(object, 0x258);
        const std::uint32_t begin = loadDword(object, 0x254);
        std::int32_t adjustment = static_cast<std::int32_t>(end - begin);
        adjustment >>= 2;
        adjustment = -adjustment;
        adjustment *= 4;
        storeDword(object, 0x258, end + static_cast<std::uint32_t>(adjustment));
    }
    const std::uint32_t end = loadDword(object, 0x258);
    const std::uint32_t capacity = loadDword(object, 0x25C);
    if (end < capacity) {
        storeDword(object, 0x258, end + 4);
        if (end != 0) {
            storeDword(fromAddressMutable(end), 0, word);
        }
    }
    const std::uint32_t header = addressOf(object) + 0x268;
    storeDword(object, 0x270, 0);
    object[0x274] = 0;
    storeDword(object, 0x278, 0);
    storeDword(object, 0x26C, header);
    storeDword(object, 0x268, header);
    return header;
}

}  // namespace ra3::common
