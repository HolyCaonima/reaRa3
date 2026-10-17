#include <ra3/gamelogic/ObjectCore6A.h>

#include <cstdio>
#include <cstring>

namespace ra3::gamelogic {
namespace {

void setTrace(ModuleResult6A& result, const char* text) noexcept
{
    std::snprintf(result.trace, sizeof(result.trace), "%s", text);
}

std::uint32_t lowByte(std::uint32_t value, std::uint32_t byte) noexcept
{
    return (value & 0xFFFFFF00u) | (byte & 0xFFu);
}

std::uint32_t readDword(const std::uint8_t module[kModule6ABytes], std::size_t offset) noexcept
{
    std::uint32_t value = 0;
    std::memcpy(&value, module + offset, sizeof(value));
    return value;
}

} // namespace

std::uint32_t encloseContainsId(
    const std::uint32_t* ids, std::uint32_t count, std::uint32_t needle,
    std::uint32_t listAddress) noexcept
{
    const std::uint32_t* cursor = ids;
    const std::uint32_t* end = ids + count;
    if (cursor != end) {
        while (*cursor != needle) {
            ++cursor;
            if (cursor == end) {
                break;
            }
        }
    }
    const std::uint32_t found = cursor != end ? 1u : 0u;
    return lowByte(listAddress, found);
}

ModuleResult6A weaponSetCallSlot1C(
    std::uint32_t incomingEax, const std::uint32_t* slotReturns, std::size_t count) noexcept
{
    ModuleResult6A result{};
    result.eax = count == 0 ? incomingEax : slotReturns[count - 1];
    if (count == 0) {
        setTrace(result, "-");
        return result;
    }
    std::size_t used = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const int wrote = std::snprintf(
            result.trace + used, sizeof(result.trace) - used, "s1c:%u;", static_cast<unsigned>(i));
        if (wrote < 0 || static_cast<std::size_t>(wrote) >= sizeof(result.trace) - used) {
            break;
        }
        used += static_cast<std::size_t>(wrote);
    }
    return result;
}

std::uint32_t slaughterContainedFlag(std::uint32_t helperEax) noexcept
{
    return helperEax == 0u ? 0u : 1u;
}

ModuleResult6A veterancyCrateCanPass(
    std::uint32_t helper6908, std::uint8_t objectFlag471, std::uint32_t objectStatus80,
    std::uint8_t templateByte74, std::uint8_t templateByte75, bool sameTeam,
    std::uint32_t objectTeam, std::uint32_t helper6875) noexcept
{
    ModuleResult6A result{};
    setTrace(result, "h6908e0;");
    if ((helper6908 & 0xFFu) == 0u || (objectFlag471 & 1u) != 0u) {
        result.eax = lowByte(helper6908, 0);
        return result;
    }
    const std::uint32_t shifted = objectStatus80 >> 7;
    if ((shifted & 1u) != 0u || templateByte74 != 0) {
        result.eax = lowByte(shifted, 0);
        return result;
    }
    if (templateByte75 == 0) {
        result.eax = lowByte(shifted, 1);
        return result;
    }
    if (!sameTeam) {
        result.eax = lowByte(objectTeam, 0);
        return result;
    }
    setTrace(result, "h6908e0;h6875a0;");
    result.eax = lowByte(helper6875, (helper6875 & 0xFFu) == 0u ? 1u : 0u);
    return result;
}

ModuleResult6A clearanceSlowDeathXfer(std::uint32_t versionEax, std::uint32_t helperEax) noexcept
{
    (void)versionEax;
    ModuleResult6A result{};
    result.eax = helperEax;
    setTrace(result, "v18;c68ecd0;");
    return result;
}

ModuleResult6A techUpgradeXfer(std::uint32_t versionEax, std::uint32_t helperEax) noexcept
{
    (void)versionEax;
    ModuleResult6A result{};
    result.eax = helperEax;
    setTrace(result, "v18;c6858e0;");
    return result;
}

ModuleResult6A dozerMoveToXfer(std::uint32_t versionEax, std::uint32_t blockEax) noexcept
{
    (void)versionEax;
    ModuleResult6A result{};
    result.eax = blockEax;
    setTrace(result, "v18;v14:20;");
    return result;
}

ModuleResult6A dozerStateMachineXfer(std::uint32_t versionEax, std::uint32_t blockEax) noexcept
{
    (void)versionEax;
    ModuleResult6A result{};
    result.eax = blockEax;
    setTrace(result, "v18;v14:80;");
    return result;
}

ModuleResult6A dozerPickXfer(
    std::uint32_t versionEax, std::uint32_t blockEax, std::uint32_t tailEax) noexcept
{
    (void)versionEax;
    (void)blockEax;
    ModuleResult6A result{};
    result.eax = tailEax;
    setTrace(result, "v18;v14:20;v70:24;");
    return result;
}

ModuleResult6A dozerDoXfer(
    std::uint32_t versionEax, std::uint32_t blockEax, std::uint32_t tailEax) noexcept
{
    (void)versionEax;
    (void)blockEax;
    ModuleResult6A result{};
    result.eax = tailEax;
    setTrace(result, "v18;v14:20;v6c:24;");
    return result;
}

ModuleResult6A dozerActionXfer(
    const std::uint8_t module[kModule6ABytes], std::uint32_t versionEax,
    std::uint32_t blockEax, std::uint32_t valueEax) noexcept
{
    (void)versionEax;
    (void)blockEax;
    ModuleResult6A result{};
    result.eax = valueEax;
    std::snprintf(
        result.trace, sizeof(result.trace), "v18;v14:20;v20:%08x;", readDword(module, 0x24));
    return result;
}

ModuleResult6A subObjectsUpgradeXfer(
    std::uint32_t versionEax, std::uint32_t blockEax, std::uint32_t localEax,
    std::uint32_t helperEax) noexcept
{
    (void)versionEax;
    (void)blockEax;
    (void)localEax;
    ModuleResult6A result{};
    result.eax = helperEax;
    setTrace(result, "v18;v6c:28;c6a1ea0;c6858e0;");
    return result;
}

ModuleResult6A experienceTrackerObjectXfer(
    std::uint32_t versionEax, std::uint32_t helper3d10, std::uint32_t helper54a090,
    std::uint32_t tailEax) noexcept
{
    (void)versionEax;
    (void)helper3d10;
    (void)helper54a090;
    ModuleResult6A result{};
    result.eax = tailEax;
    setTrace(result, "v18;c6a3d10;c54a090:3c;v84:40;");
    return result;
}

ModuleResult6A audioLoopUpgradeXfer(
    std::uint32_t versionEax, std::uint32_t globalEax, std::uint32_t baseEax,
    std::uint32_t subEax) noexcept
{
    (void)versionEax;
    (void)globalEax;
    (void)baseEax;
    ModuleResult6A result{};
    result.eax = subEax;
    setTrace(result, "v18;g158:30;c6858e0;c685080:18;");
    return result;
}

ModuleResult6A createAndEnterXfer(
    std::uint32_t versionEax0, std::uint32_t versionEax1, std::uint32_t blockEax,
    std::uint32_t helperEax) noexcept
{
    (void)versionEax0;
    (void)versionEax1;
    (void)blockEax;
    ModuleResult6A result{};
    result.eax = helperEax;
    setTrace(result, "v18;v18;v6c:50;c681220;");
    return result;
}

ModuleResult6A techUpgradeReceiverXfer(
    std::uint32_t xferPlus4, std::uint32_t versionEax, std::uint32_t baseEax,
    std::uint32_t helper54a090, std::uint32_t slot64a, std::uint32_t slot64b,
    std::uint32_t slot70) noexcept
{
    (void)versionEax;
    (void)baseEax;
    (void)slot64a;
    (void)slot64b;
    ModuleResult6A result{};
    if (((xferPlus4 >> 2) & 1u) != 0u) {
        result.eax = helper54a090;
        setTrace(result, "v18;c6858e0;c54a090:30;");
        return result;
    }
    result.eax = slot70;
    setTrace(result, "v18;c6858e0;c54a090:30;v64:38;v64:34;v70:3c;");
    return result;
}

} // namespace ra3::gamelogic
