#include <ra3/common/CommonSubsystem.h>

#include <cstring>

namespace ra3::common {
namespace {

std::uint32_t loadU32(const void* address) noexcept
{
    std::uint32_t value = 0;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

const std::uint8_t* loadPtr(const void* address) noexcept
{
    return reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(loadU32(address)));
}

}  // namespace

const std::uint8_t* objectBytes(const CommonSubsystem* object) noexcept
{
    return reinterpret_cast<const std::uint8_t*>(object);
}

std::uint32_t CommonSubsystem::disguiseGuardName() const noexcept { return 0x00C34910U; }
std::uint32_t CommonSubsystem::engagementTrackerName() const noexcept { return 0x00C34C44U; }
std::uint32_t CommonSubsystem::redAlertName() const noexcept { return 0x00C34C7CU; }
std::uint32_t CommonSubsystem::scoredKillTrackerName() const noexcept { return 0x00C34C9CU; }
std::uint32_t CommonSubsystem::moneyName() const noexcept { return 0x00C34EE0U; }
std::uint32_t CommonSubsystem::commandName() const noexcept { return 0x00C34EF8U; }
std::uint32_t CommonSubsystem::playerListName() const noexcept { return 0x00C34FDCU; }
std::uint32_t CommonSubsystem::energyName() const noexcept { return 0x00C34FF8U; }
std::uint32_t CommonSubsystem::playerRelationMapName() const noexcept { return 0x00C35038U; }
std::uint32_t CommonSubsystem::playerTechManagerName() const noexcept { return 0x00C3505CU; }
std::uint32_t CommonSubsystem::resourceGatheringManagerName() const noexcept { return 0x00C350D0U; }
std::uint32_t CommonSubsystem::teamRelationMapName() const noexcept { return 0x00C3510CU; }
std::uint32_t CommonSubsystem::tunnelTrackerName() const noexcept { return 0x00C35168U; }
std::uint32_t CommonSubsystem::playerTeamListName() const noexcept { return 0x00C351C4U; }
std::uint32_t CommonSubsystem::teamFactoryName() const noexcept { return 0x00C35224U; }
std::uint32_t CommonSubsystem::playerName() const noexcept { return 0x00C35340U; }
std::uint32_t CommonSubsystem::teamTemplateInfoName() const noexcept { return 0x00C35390U; }
std::uint32_t CommonSubsystem::teamName() const noexcept { return 0x00BD1240U; }
std::uint32_t CommonSubsystem::teamPrototypeName() const noexcept { return 0x00C353ECU; }
std::uint32_t CommonSubsystem::scoreKeeperName() const noexcept { return 0x00C35500U; }
std::uint32_t CommonSubsystem::logicEvaTrackerName() const noexcept { return 0x00C35A8CU; }
std::uint32_t CommonSubsystem::logicPlayingAudioTrackerName() const noexcept { return 0x00C35AC0U; }
std::uint32_t CommonSubsystem::logicViewTrackerName() const noexcept { return 0x00C35ADCU; }

std::uint32_t CommonSubsystem::linkedTextLength() const noexcept
{
    const auto* first = loadPtr(objectBytes(this) + 0x04);
    const auto* second = loadPtr(first);
    const auto* buffer = loadPtr(second);
    if (buffer == nullptr) {
        return 0;
    }
    std::uint16_t length = 0;
    std::memcpy(&length, buffer - 4, sizeof(length));
    return length;
}

std::uint32_t CommonSubsystem::linkedTextLengthPlus() const noexcept
{
    const auto* node = loadPtr(objectBytes(this) + 0x04);
    const std::uint32_t extra = loadU32(node + 0x04);
    const auto* link = loadPtr(node + 0x08);
    const auto* buffer = loadPtr(link);
    if (buffer == nullptr) {
        return extra;
    }
    std::uint16_t length = 0;
    std::memcpy(&length, buffer - 4, sizeof(length));
    return extra + length;
}

#if defined(_MSC_VER) && defined(_M_IX86)
#pragma runtime_checks("", off)

__declspec(naked) void CommonSubsystem::adjustTunnelCount(std::uint8_t) noexcept
{
    __asm {
        cmp byte ptr [esp + 4], 0
        je negative
        add dword ptr [ecx + 10h], 1
        ret 4
    negative:
        add dword ptr [ecx + 10h], -1
        ret 4
    }
}

__declspec(naked) std::uint32_t CommonSubsystem::copyNestedTriple(std::uint32_t*) const noexcept
{
    __asm {
        mov eax, dword ptr [ecx + 8Ch]
        test eax, eax
        je missing
        mov edx, dword ptr [eax + 44h]
        mov ecx, dword ptr [esp + 4]
        mov dword ptr [ecx], edx
        mov edx, dword ptr [eax + 48h]
        mov dword ptr [ecx + 4], edx
        mov edx, dword ptr [eax + 4Ch]
        mov dword ptr [ecx + 8], edx
        mov al, byte ptr [eax + 50h]
        ret 4
    missing:
        xor al, al
        ret 4
    }
}

__declspec(naked) std::uint32_t CommonSubsystem::forwardNested34(std::uint32_t, std::uint32_t) const noexcept
{
    __asm {
        cmp dword ptr [ecx + 8Ch], 0
        je missing
        mov ecx, dword ptr [ecx + 8Ch]
        mov eax, dword ptr [ecx]
        mov eax, dword ptr [eax + 34h]
        jmp eax
    missing:
        xor al, al
        ret 8
    }
}

__declspec(naked) std::uint32_t CommonSubsystem::forwardNested38(std::uint32_t) const noexcept
{
    __asm {
        cmp dword ptr [ecx + 8Ch], 0
        je missing
        mov ecx, dword ptr [ecx + 8Ch]
        mov eax, dword ptr [ecx]
        mov eax, dword ptr [eax + 38h]
        jmp eax
    missing:
        ret 4
    }
}

__declspec(naked) std::uint32_t CommonSubsystem::storeFlagCf(std::uint32_t) noexcept
{
    __asm {
        mov al, byte ptr [esp + 4]
        mov byte ptr [ecx + 0CFh], al
        ret 4
    }
}

__declspec(naked) std::uint32_t CommonSubsystem::byteAt40() const noexcept
{
    __asm {
        mov al, byte ptr [ecx + 40h]
        ret
    }
}

__declspec(naked) std::uint32_t CommonSubsystem::byteAtD0() const noexcept
{
    __asm {
        mov al, byte ptr [ecx + 0D0h]
        ret
    }
}

__declspec(naked) std::uint32_t CommonSubsystem::byteB8UnlessBa() const noexcept
{
    __asm {
        cmp byte ptr [ecx + 0BAh], 0
        je take
        xor al, al
        ret
    take:
        mov al, byte ptr [ecx + 0B8h]
        ret
    }
}

__declspec(naked) std::uint32_t CommonSubsystem::byteB8UnlessBaMutant() const noexcept
{
    __asm {
        cmp byte ptr [ecx + 0BAh], 0
        jne take
        xor al, al
        ret
    take:
        mov al, byte ptr [ecx + 0B8h]
        ret
    }
}

__declspec(naked) std::uint32_t CommonSubsystem::availabilityByte() const noexcept
{
    __asm {
        cmp byte ptr [ecx + 0BAh], 0
        je indexed
        cmp dword ptr [ecx + 0BCh], 0
        jle none
        mov al, byte ptr [ecx + 0B9h]
        ret
    indexed:
        mov eax, dword ptr [ecx + 9Ch]
        test eax, eax
        jl none
        cmp eax, 14h
        jae none
        mov edx, dword ptr [ecx]
        mov eax, dword ptr [ecx + eax*4 + 4Ch]
        mov edx, dword ptr [edx + 44h]
        push eax
        call edx
        ret
    none:
        xor al, al
        ret
    }
}

#pragma runtime_checks("", restore)
#endif

}  // namespace ra3::common
