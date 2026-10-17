#include <ra3/gameclient/DisplayExtent.h>

#include <cstring>

namespace ra3::gameclient {
namespace {

std::uint32_t readDword(const void* object, std::size_t offset) noexcept
{
    std::uint32_t value = 0;
    std::memcpy(&value, static_cast<const std::uint8_t*>(object) + offset, sizeof(value));
    return value;
}

void writeDword(void* object, std::size_t offset, std::uint32_t value) noexcept
{
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
}

}  // namespace

std::uint32_t DisplayExtentObject::loadPrimaryExtent() const noexcept
{
    return readDword(this, 0x4C);
}

std::uint32_t DisplayExtentObject::storePrimaryExtent(std::uint32_t value) noexcept
{
    writeDword(this, 0x4C, value);
    return value;
}

std::uint32_t DisplayExtentObject::loadSecondaryExtent() const noexcept
{
    return readDword(this, 0x50);
}

std::uint32_t DisplayExtentObject::storeSecondaryExtent(std::uint32_t value) noexcept
{
    writeDword(this, 0x50, value);
    return value;
}

std::uint32_t DisplayExtentObject::secondaryTimesFourBelowPrimaryTimesThree() const noexcept
{
    // 0x0052AFA0 reads +0x50 first, doubles it twice, triples +0x4C with
    // lea, then cmp / sbb eax,eax / neg eax.  sbb yields 0 or -1 from CF,
    // and neg turns that into 0 or 1.  CF is set only when the wrapped
    // 4*secondary is below the wrapped 3*primary.
    const std::uint32_t secondary = readDword(this, 0x50);
    const std::uint32_t primary = readDword(this, 0x4C);
    const std::uint32_t scaledSecondary = secondary * 4U;
    const std::uint32_t scaledPrimary = primary * 3U;
    return scaledSecondary < scaledPrimary ? 1U : 0U;
}

std::uint32_t secondaryTimesFourBelowPrimaryTimesThreeMutant(
    const DisplayExtentObject& object) noexcept
{
    const std::uint32_t secondary = readDword(&object, 0x50);
    const std::uint32_t primary = readDword(&object, 0x4C);
    const std::uint32_t scaledSecondary = secondary * 4U;
    const std::uint32_t scaledPrimary = primary * 3U;
    return scaledSecondary < scaledPrimary ? 0U : 1U;
}

std::uint32_t DisplayExtentObject::storeTriple64(const std::uint32_t* source) noexcept
{
    auto* base = reinterpret_cast<std::uint8_t*>(this);
    const std::uint32_t first = source[0];
    writeDword(base, 0x64, first);
    const std::uint32_t second = source[1];
    writeDword(base, 0x68, second);
    const std::uint32_t third = source[2];
    writeDword(base, 0x6C, third);
    return third;
}

std::uint32_t DisplayExtentObject::storePair74(
    std::uint32_t first, std::uint32_t second) noexcept
{
    writeDword(this, 0x74, first);
    writeDword(this, 0x78, second);
    return first;
}

std::uint32_t DisplayExtentObject::loadField74() const noexcept
{
    return readDword(this, 0x74);
}

std::uint32_t DisplayExtentObject::storeFieldCC(std::uint32_t value) noexcept
{
    writeDword(this, 0xCC, value);
    return value;
}

std::uint32_t DisplayExtentObject::loadField110() const noexcept
{
    return readDword(this, 0x110);
}

std::uint32_t DisplayExtentObject::storeField110(std::uint32_t value) noexcept
{
    writeDword(this, 0x110, value);
    return value;
}

std::uint32_t DisplayExtentObject::storeQuad11C(const std::uint32_t* source) noexcept
{
    auto* base = reinterpret_cast<std::uint8_t*>(this);
    const std::uint32_t first = source[0];
    writeDword(base, 0x11C, first);
    const std::uint32_t second = source[1];
    writeDword(base, 0x120, second);
    const std::uint32_t third = source[2];
    writeDword(base, 0x124, third);
    const std::uint32_t fourth = source[3];
    writeDword(base, 0x128, fourth);
    base[0x12C] = 1;
    return fourth;
}

std::uint32_t DisplayExtentObject::loadField118() const noexcept
{
    return readDword(this, 0x118);
}

#if defined(_MSC_VER) && defined(_M_IX86)
#pragma runtime_checks("", off)

__declspec(naked) std::uint32_t DisplayExtentObject::loadFlag5E() const noexcept
{
    __asm {
        mov al, byte ptr [ecx + 5Eh]
        ret
    }
}

__declspec(naked) std::uint32_t DisplayExtentObject::storeFlag5F(std::uint32_t) noexcept
{
    __asm {
        mov al, byte ptr [esp + 4]
        mov byte ptr [ecx + 5Fh], al
        ret 4
    }
}

__declspec(naked) std::uint32_t DisplayExtentObject::loadFlag5F() const noexcept
{
    __asm {
        mov al, byte ptr [ecx + 5Fh]
        ret
    }
}

__declspec(naked) std::uint32_t DisplayExtentObject::storeFlag60(std::uint32_t) noexcept
{
    __asm {
        mov al, byte ptr [esp + 4]
        mov byte ptr [ecx + 60h], al
        ret 4
    }
}

__declspec(naked) std::uint32_t DisplayExtentObject::loadFlag60() const noexcept
{
    __asm {
        mov al, byte ptr [ecx + 60h]
        ret
    }
}

__declspec(naked) std::uint32_t DisplayExtentObject::loadFlag12C() const noexcept
{
    __asm {
        mov al, byte ptr [ecx + 12Ch]
        ret
    }
}

__declspec(naked) std::uint32_t DisplayExtentObject::storeFlag12C(std::uint32_t) noexcept
{
    __asm {
        mov al, byte ptr [esp + 4]
        mov byte ptr [ecx + 12Ch], al
        ret 4
    }
}

#pragma runtime_checks("", restore)
#else
namespace {

std::uint32_t readByte(const void* object, std::size_t offset) noexcept
{
    return static_cast<const std::uint8_t*>(object)[offset];
}

}  // namespace

std::uint32_t DisplayExtentObject::loadFlag5E() const noexcept
{
    return readByte(this, 0x5E);
}

std::uint32_t DisplayExtentObject::storeFlag5F(std::uint32_t value) noexcept
{
    reinterpret_cast<std::uint8_t*>(this)[0x5F] = static_cast<std::uint8_t>(value);
    return value & 0xFFU;
}

std::uint32_t DisplayExtentObject::loadFlag5F() const noexcept
{
    return readByte(this, 0x5F);
}

std::uint32_t DisplayExtentObject::storeFlag60(std::uint32_t value) noexcept
{
    reinterpret_cast<std::uint8_t*>(this)[0x60] = static_cast<std::uint8_t>(value);
    return value & 0xFFU;
}

std::uint32_t DisplayExtentObject::loadFlag60() const noexcept
{
    return readByte(this, 0x60);
}

std::uint32_t DisplayExtentObject::loadFlag12C() const noexcept
{
    return readByte(this, 0x12C);
}

std::uint32_t DisplayExtentObject::storeFlag12C(std::uint32_t value) noexcept
{
    reinterpret_cast<std::uint8_t*>(this)[0x12C] = static_cast<std::uint8_t>(value);
    return value & 0xFFU;
}
#endif

}  // namespace ra3::gameclient
