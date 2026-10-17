#include <ra3/gameclient/DrawableAccess.h>

#include <cstring>
#include <xmmintrin.h>

namespace ra3::gameclient {
namespace {

std::uint32_t load32(const void* base, std::uint32_t offset) noexcept
{
    std::uint32_t value = 0;
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, 4);
    return value;
}

void store32(void* base, std::uint32_t offset, std::uint32_t value) noexcept
{
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, 4);
}

float loadFloat(const void* base, std::uint32_t offset) noexcept
{
    float value = 0.f;
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, 4);
    return value;
}

void storeFloat(void* base, std::uint32_t offset, float value) noexcept
{
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, 4);
}

std::uint8_t* raw(void* base) noexcept
{
    return static_cast<std::uint8_t*>(base);
}

float bitsToFloat(std::uint32_t bits) noexcept
{
    float value = 0.f;
    std::memcpy(&value, &bits, 4);
    return value;
}

std::uint32_t pointerValue(const void* pointer) noexcept
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

void* pointerFrom(std::uint32_t value) noexcept
{
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(value));
}

void** listAt(void* self, std::uint32_t offset) noexcept
{
    return static_cast<void**>(pointerFrom(load32(self, offset)));
}

struct ZeroCompare {
    std::uint32_t eax;
    bool jumpParity;
};

// ucomiss + lahf + `test ah, 0x44` + jp.  JP is taken unless the value is
// ordered-equal to zero.  AH is the lahf image of the incoming EAX.
ZeroCompare compareWithZero(std::uint32_t incoming, float value) noexcept
{
    std::uint32_t eaxOut = incoming;
    std::uint32_t parity = 0;
    __asm {
        mov eax, incoming
        movss xmm1, value
        xorps xmm0, xmm0
        ucomiss xmm1, xmm0
        lahf
        test ah, 0x44
        jp taken
        mov parity, 0
        jmp store
    taken:
        mov parity, 1
    store:
        mov eaxOut, eax
    }
    ZeroCompare result;
    result.eax = eaxOut;
    result.jumpParity = parity != 0;
    return result;
}

__m128 negativeZero() noexcept
{
    alignas(16) std::uint32_t bits[4] = {kNegativeZeroBits, 0, 0, 0};
    __m128 value = _mm_setzero_ps();
    std::memcpy(&value, bits, sizeof(value));
    return value;
}

void addSin(float& phase, float addend, float amplitude, float* point) noexcept
{
    __m128 current = _mm_load_ss(&phase);
    current = _mm_add_ss(current, _mm_load_ss(&addend));
    _mm_store_ss(&phase, current);
    float stored = phase;
    __asm {
        fld stored
        fsin
        fmul amplitude
        mov eax, point
        fadd dword ptr [eax]
        fstp dword ptr [eax]
    }
}

void addCos(float& phase, float addend, float amplitude, float* point) noexcept
{
    __m128 current = _mm_load_ss(&phase);
    current = _mm_add_ss(current, _mm_load_ss(&addend));
    _mm_store_ss(&phase, current);
    float stored = phase;
    __asm {
        fld stored
        fcos
        fmul amplitude
        mov eax, point
        fadd dword ptr [eax]
        fstp dword ptr [eax]
    }
}

bool absoluteExceedsHalfPi(float angle) noexcept
{
    const float halfPi = bitsToFloat(kHalfPiBits);
    std::uint32_t above = 0;
    __asm {
        fld angle
        fabs
        fld halfPi
        fxch st(1)
        fcomip st(0), st(1)
        fstp st(0)
        jbe within
        mov above, 1
        jmp done
    within:
        mov above, 0
    done:
    }
    return above != 0;
}

std::uint32_t walkSlot(
    void** list, std::uint32_t slot, const std::uint32_t* args, int argCount,
    DrawableServices& services, std::uint32_t eax)
{
    void* element = *list;
    while (element != nullptr) {
        eax = services.callSlot(element, slot, args, argCount, eax);
        ++list;
        element = *list;
    }
    return eax;
}

std::uint8_t withBit1(std::uint8_t current, std::uint8_t bit) noexcept
{
    std::uint8_t updated = static_cast<std::uint8_t>(bit + bit);
    updated = static_cast<std::uint8_t>(updated ^ current);
    updated = static_cast<std::uint8_t>(updated & 2u);
    return static_cast<std::uint8_t>(updated ^ current);
}

void constructSwayState(void* state, DrawableServices& services)
{
    std::memset(state, 0, 0x70);
    store32(state, 0, kSwayStateVptr);
    const float high = bitsToFloat(kRandomHighBits);
    storeFloat(state, 0x34, services.randomReal(0.f, high, 0x17D));
    storeFloat(state, 0x38, services.randomReal(0.f, high, 0x17E));
}

float clampCross(float value) noexcept
{
    const float low = bitsToFloat(kSwayClampLowBits);
    const float high = bitsToFloat(kSwayClampHighBits);
    if (low > value) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

float clampSymmetric(float value, float limit) noexcept
{
    if (value > limit) {
        return limit;
    }
    __m128 span = _mm_sub_ss(negativeZero(), _mm_set_ss(limit));
    float floor = 0.f;
    _mm_store_ss(&floor, span);
    if (floor > value) {
        return floor;
    }
    return value;
}

}  // namespace

std::uint32_t tintEnvelopeStatic() noexcept
{
    return kTintEnvelopeStatic;
}

std::uint32_t copyTwelveDwords(void* destination, const void* source) noexcept
{
    for (std::uint32_t offset = 0; offset < 0x30; offset += 4) {
        const float* from = reinterpret_cast<const float*>(
            static_cast<const std::uint8_t*>(source) + offset);
        float* to = reinterpret_cast<float*>(raw(destination) + offset);
        __asm {
            mov eax, from
            mov ecx, to
            fld dword ptr [eax]
            fstp dword ptr [ecx]
        }
    }
    return pointerValue(destination);
}

std::uint32_t positionPointer(void* self, PositionGlobals* globals) noexcept
{
    if (load32(self, 0x130) != 0xFFFFFFFFu) {
        return pointerValue(raw(self) + 0x124);
    }
    if ((globals->ready & 1u) == 0) {
        globals->ready |= 1u;
        globals->xyz[0] = 0.f;
        globals->xyz[1] = 0.f;
        globals->xyz[2] = 0.f;
    }
    return pointerValue(globals);
}

std::uint32_t transformPointer(void* self, TransformGlobals* globals) noexcept
{
    if (load32(self, 0x130) != 0xFFFFFFFFu) {
        return pointerValue(raw(self) + 0xF4);
    }
    if ((globals->ready & 1u) == 0) {
        const float one = bitsToFloat(kIdentityDiagonal);
        const float zero = 0.f;
        globals->ready |= 1u;
        globals->cells[0] = one;
        globals->cells[1] = zero;
        globals->cells[2] = zero;
        globals->cells[3] = zero;
        globals->cells[4] = zero;
        globals->cells[5] = one;
        globals->cells[6] = zero;
        globals->cells[7] = zero;
        globals->cells[8] = zero;
        globals->cells[9] = zero;
        globals->cells[10] = one;
        globals->cells[11] = zero;
    }
    return pointerValue(globals);
}

std::uint32_t visitModules20(void* self, DrawableServices& services, std::uint32_t eax)
{
    return walkSlot(listAt(self, 0x178), 0x20, nullptr, 0, services, eax);
}

std::uint32_t queryModulesB0(
    void* self, std::uint32_t argument, DrawableServices& services, std::uint32_t eax)
{
    void** list = listAt(self, 0x178);
    void* element = *list;
    while (element != nullptr) {
        eax = services.callSlot(element, 0xB0, nullptr, 0, eax);
        if (eax != 0) {
            eax = services.callSlot(pointerFrom(eax), 0x70, &argument, 1, eax);
        }
        ++list;
        element = *list;
    }
    return eax;
}

std::uint32_t publishScalar(
    void* self, std::uint32_t arg0, std::uint32_t arg1, std::uint32_t floatBits,
    DrawableServices& services, std::uint32_t eax)
{
    raw(self)[0x13F] = static_cast<std::uint8_t>(raw(self)[0x13F] | kScalarPublishBit);
    const std::uint32_t args[3] = {arg0, arg1, floatBits};
    void** first = listAt(self, 0x178);
    void* element = *first;
    while (element != nullptr) {
        eax = services.callSlot(element, 0xA4, args, 3, eax);
        ++first;
        element = *first;
    }
    void** second = listAt(self, 0x180);
    if (second == nullptr) {
        return eax;
    }
    element = *second;
    while (element != nullptr) {
        eax = services.callSlot(element, 0x30, args, 3, eax);
        ++second;
        element = *second;
    }
    return eax;
}

std::uint32_t publishCondition(void* self, DrawableServices& services, std::uint32_t eax)
{
    const std::uint8_t selected = raw(self)[0x25B];
    eax = (eax & 0xFFFFFF00u) | selected;
    std::uint32_t published = selected;
    if (selected == 0) {
        if ((raw(self)[0x13D] & 0x40) != 0) {
            void* object = pointerFrom(load32(self, 0x138));
            if (object != nullptr) {
                eax = services.callDirect(0x006A54D0, object, nullptr, 0, eax);
                if ((eax & 0xFF) != 0) {
                    published = 1;
                }
            }
        }
        if (published == 0) {
            published = 2;
            if ((raw(self)[0x13E] & 0x40) == 0) {
                published = raw(self)[0x25A];
            }
        }
    }
    return walkSlot(listAt(self, 0x178), 0x90, &published, 1, services, eax);
}

std::uint32_t publishArgument(
    void* self, std::uint32_t argument, DrawableServices& services, std::uint32_t eax)
{
    void** first = listAt(self, 0x178);
    eax = pointerValue(first);
    if (*first != nullptr) {
        eax = services.callSlot(*first, 0x80, &argument, 1, eax);
    }
    void** second = listAt(self, 0x180);
    if (second == nullptr) {
        return eax;
    }
    void* element = *second;
    while (element != nullptr) {
        eax = services.callSlot(element, 0x50, nullptr, 0, eax);
        ++second;
        element = *second;
    }
    return eax;
}

std::uint32_t addSwayOffset(void* self, void* record, void* point, std::uint32_t eax)
{
    void* inner = pointerFrom(load32(pointerFrom(load32(record, 4)), 4));
    const float amplitudeY = loadFloat(inner, 0x104);
    const float amplitudeX = loadFloat(inner, 0xFC);
    const float addY = loadFloat(inner, 0x108);
    const float addX = loadFloat(inner, 0x100);
    ZeroCompare first = compareWithZero(pointerValue(inner), amplitudeY);
    eax = first.eax;
    bool applyY = first.jumpParity;
    if (!applyY) {
        ZeroCompare second = compareWithZero(eax, addY);
        eax = second.eax;
        applyY = second.jumpParity;
    }
    if (applyY) {
        void* sway = pointerFrom(load32(self, 0x208));
        float phase = loadFloat(sway, 0x34);
        float pointY = loadFloat(point, 4);
        addSin(phase, addY, amplitudeY, &pointY);
        storeFloat(sway, 0x34, phase);
        storeFloat(point, 4, pointY);
        eax = pointerValue(sway);
    }
    ZeroCompare third = compareWithZero(eax, amplitudeX);
    eax = third.eax;
    bool applyX = third.jumpParity;
    if (!applyX) {
        ZeroCompare fourth = compareWithZero(eax, addX);
        eax = fourth.eax;
        applyX = fourth.jumpParity;
    }
    if (applyX) {
        void* sway = pointerFrom(load32(self, 0x208));
        float phase = loadFloat(sway, 0x38);
        float pointX = loadFloat(point, 0);
        addCos(phase, addX, amplitudeX, &pointX);
        storeFloat(sway, 0x38, phase);
        storeFloat(point, 0, pointX);
        eax = pointerValue(sway);
    }
    return eax;
}

std::uint32_t integrateSwayAngle(void* self, void* record, float sample, std::uint32_t eax)
{
    void* middle = pointerFrom(load32(record, 4));
    eax = pointerValue(middle);
    void* inner = pointerFrom(load32(middle, 4));
    const float rate = loadFloat(inner, 0xA4);
    ZeroCompare first = compareWithZero(eax, rate);
    eax = first.eax;
    if (!first.jumpParity) {
        return eax;
    }
    const float damping = loadFloat(inner, 0xA8);
    ZeroCompare second = compareWithZero(eax, damping);
    eax = second.eax;
    if (!second.jumpParity) {
        return eax;
    }
    void* sway = pointerFrom(load32(self, 0x208));
    __m128 angle = _mm_load_ss(reinterpret_cast<float*>(raw(sway) + 0x18));
    angle = _mm_add_ss(angle, _mm_mul_ss(_mm_load_ss(&rate), _mm_set_ss(sample)));
    _mm_store_ss(reinterpret_cast<float*>(raw(sway) + 0x18), angle);
    const float one = bitsToFloat(kIdentityDiagonal);
    __m128 scale = _mm_sub_ss(_mm_set_ss(one), _mm_load_ss(&damping));
    angle = _mm_mul_ss(scale, _mm_load_ss(reinterpret_cast<float*>(raw(sway) + 0x18)));
    _mm_store_ss(reinterpret_cast<float*>(raw(sway) + 0x18), angle);
    float stored = loadFloat(sway, 0x18);
    if (absoluteExceedsHalfPi(stored)) {
        if (stored > 0.f) {
            storeFloat(sway, 0x18, bitsToFloat(kHalfPiBits));
        } else {
            __m128 negative = _mm_sub_ss(negativeZero(), _mm_set_ss(bitsToFloat(kHalfPiBits)));
            _mm_store_ss(reinterpret_cast<float*>(raw(sway) + 0x18), negative);
        }
    }
    return pointerValue(sway);
}

std::uint32_t assignByte25A(
    void* self, std::uint32_t value, DrawableServices& services, std::uint32_t eax)
{
    (void)eax;
    if (value == raw(self)[0x25A]) {
        return value;
    }
    raw(self)[0x25A] = static_cast<std::uint8_t>(value);
    return publishCondition(self, services, value);
}

std::uint32_t assignByte25B(
    void* self, std::uint32_t value, DrawableServices& services, std::uint32_t eax)
{
    (void)eax;
    if (value == raw(self)[0x25B]) {
        return value;
    }
    raw(self)[0x25B] = static_cast<std::uint8_t>(value);
    return publishCondition(self, services, value);
}

std::uint32_t ensureAuxRecord(void* self, DrawableServices& services)
{
    if (load32(self, 0x328) != 0) {
        return load32(self, 0x328);
    }
    void* created = services.allocate(0x24);
    if (created == nullptr) {
        store32(self, 0x328, 0);
        return 0;
    }
    store32(created, 0, kAuxObjectVptr);
    store32(created, 4, 0);
    store32(created, 0x14, 0);
    store32(created, 8, 0);
    store32(created, 0x18, 0);
    store32(created, 0xC, 0);
    store32(created, 0x1C, 0);
    store32(created, 0x10, 0);
    store32(created, 0x20, 0);
    store32(self, 0x328, pointerValue(created));
    return pointerValue(created);
}

std::uint32_t releaseTracked(
    void* self, void* registry, DrawableServices& services, std::uint32_t eax)
{
    const std::uint32_t previous = load32(self, 0x20C);
    eax = services.callSlot(registry, 0x6C, &previous, 1, eax);
    store32(self, 0x20C, 1);
    void* tracked = pointerFrom(load32(self, 0x1F4));
    if (tracked == nullptr) {
        return eax;
    }
    std::uint32_t old = load32(tracked, 4);
    store32(tracked, 4, old + 0xFFFFFFFFu);
    eax = old - 1u;
    if (static_cast<std::int32_t>(eax) <= 0 && tracked != nullptr) {
        const std::uint32_t flag = 1;
        eax = services.callSlot(tracked, 0, &flag, 1, eax);
    }
    store32(self, 0x1F4, 0);
    return eax;
}

std::uint32_t clearAttachedBit(
    void* self, void* registry, DrawableServices& services, std::uint32_t eax)
{
    const std::uint8_t flags = raw(self)[0x13D];
    eax = (eax & 0xFFFFFF00u) | flags;
    if ((flags & 0x10) == 0) {
        return eax;
    }
    raw(self)[0x13D] = static_cast<std::uint8_t>(flags & 0xEF);
    const std::uint32_t previous = load32(self, 0x210);
    services.callSlot(registry, 0x6C, &previous, 1, eax);
    store32(self, 0x210, 1);
    eax = releaseTracked(self, registry, services, eax);
    void** modules = listAt(self, 0x180);
    if (modules != nullptr) {
        void* element = *modules;
        while (element != nullptr) {
            eax = services.callSlot(element, 0x34, nullptr, 0, eax);
            if (eax != 0) {
                eax = services.callSlot(pointerFrom(eax), 4, nullptr, 0, eax);
            }
            ++modules;
            element = *modules;
        }
    }
    void** clients = listAt(self, 0x17C);
    if (clients == nullptr) {
        return eax;
    }
    void* element = *clients;
    while (element != nullptr) {
        eax = services.callSlot(element, 0x3C, nullptr, 0, eax);
        if (eax != 0) {
            eax = services.callSlot(pointerFrom(eax), 4, nullptr, 0, eax);
        }
        ++clients;
        element = *clients;
    }
    return eax;
}

std::uint32_t readTintEnvelope(
    void* self, void* xfer, DrawableServices& services, std::uint32_t eax)
{
    eax = load32(xfer, 4) >> 3;
    if ((eax & 1u) != 0) {
        return eax;
    }
    std::uint32_t version = 0;
    raw(reinterpret_cast<void*>(&version))[0] = 1;
    raw(reinterpret_cast<void*>(&version))[1] = 2;
    std::uint32_t versionArg = pointerValue(&version);
    eax = services.callSlot(xfer, 0x18, &versionArg, 1, eax);
    const std::uint32_t reads[] = {4, 0x10, 0x1C, 0x28};
    for (std::uint32_t offset : reads) {
        std::uint32_t dest = pointerValue(raw(self) + offset);
        eax = services.callSlot(xfer, 0x58, &dest, 1, eax);
    }
    std::uint32_t at34 = pointerValue(raw(self) + 0x34);
    eax = services.callSlot(xfer, 0x6C, &at34, 1, eax);
    std::uint32_t at39 = pointerValue(raw(self) + 0x39);
    eax = services.callSlot(xfer, 0x84, &at39, 1, eax);
    std::uint32_t at38 = pointerValue(raw(self) + 0x38);
    eax = services.callSlot(xfer, 0x80, &at38, 1, eax);
    if (raw(reinterpret_cast<void*>(&version))[1] < 2) {
        return eax;
    }
    std::uint32_t at3C = pointerValue(raw(self) + 0x3C);
    eax = services.callSlot(xfer, 0x64, &at3C, 1, eax);
    std::uint32_t at40 = pointerValue(raw(self) + 0x40);
    eax = services.callSlot(xfer, 0x64, &at40, 1, eax);
    std::uint32_t at44 = pointerValue(raw(self) + 0x44);
    return services.callSlot(xfer, 0x58, &at44, 1, eax);
}

std::uint32_t readCountedBlock(
    void* self, void* xfer, void* counter, DrawableServices& services)
{
    (void)self;
    std::uint32_t local = load32(counter, 4);
    store32(&local, 0, local);
    std::uint32_t localArg = pointerValue(&local);
    services.callSlot(xfer, 0x6C, &localArg, 1, 0);
    if (local != load32(counter, 4)) {
        void* vtable = pointerFrom(load32(xfer, 0));
        const std::uint32_t args[2] = {pointerValue(xfer), 0x00BDFEAC};
        services.callSlot(vtable, 0x8C, args, 2, 0);
    }
    if (load32(counter, 4) == 0) {
        store32(counter, 4, load32(counter, 4) - 1u);
    } else {
        for (;;) {
            store32(counter, 4, load32(counter, 4) - 1u);
            if (raw(xfer)[8] != 0) {
                break;
            }
            const std::uint32_t cursor = load32(counter, 0);
            store32(counter, 0, cursor + 0xC);
            services.callSlot(xfer, 0x58, &cursor, 1, 0);
            if (load32(counter, 4) == 0) {
                store32(counter, 4, load32(counter, 4) - 1u);
                break;
            }
        }
    }
    return pointerValue(xfer);
}

std::uint32_t resolveRecord(void* self, std::uint32_t key, DrawableServices& services)
{
    if (key == 0xFFFFFFFFu) {
        return kMissingRecord;
    }
    void** list = listAt(self, 0x180);
    if (list != nullptr) {
        void* element = *list;
        while (element != nullptr) {
            const std::uint32_t found = services.callSlot(element, 0x38, nullptr, 0, 0);
            if (found != 0) {
                const std::uint32_t match =
                    services.callSlot(pointerFrom(found), 0xC, &key, 1, found);
                if (match != 0) {
                    return match;
                }
            }
            ++list;
            element = *list;
        }
    }
    void* owner = pointerFrom(load32(self, 4));
    if (owner == nullptr) {
        return kMissingRecord;
    }
    void* table = raw(owner) + 0x15C;
    return services.callDirect(0x0055BE60, table, &key, 1, 0);
}

std::uint32_t selectRecordKey(void* self, std::uint32_t mode, DrawableServices& services)
{
    if (mode != 3) {
        const std::uint32_t field = pointerValue(raw(self) + 0x264);
        const std::uint32_t answer = services.callDirect(0x0052CD70, self, nullptr, 0, field);
        if ((answer & 0xFF) != 0) {
            return resolveRecord(self, 4, services);
        }
    }
    std::uint32_t key = 4;
    if (mode == 1) {
        key = 5;
    } else if (mode == 2) {
        key = 6;
    } else if (mode == 3) {
        key = 7;
    }
    return resolveRecord(self, key, services);
}

std::uint32_t refreshModuleGate(
    void* self, void* playerRoster, void* nameRegistry, DrawableServices& services,
    std::uint32_t eax)
{
    std::uint32_t flag = 0;
    const bool blocked = (raw(self)[0x13C] & 6) == 0 || load32(self, 0x1C8) != 0;
    if (!blocked) {
        flag = 1;
        void* object = pointerFrom(load32(self, 0x138));
        if ((raw(self)[0x13D] & 0x40) != 0 && object != nullptr && playerRoster != nullptr) {
            const std::uint32_t rosterId = load32(playerRoster, 0x2C);
            const std::uint32_t player = services.callDirect(0x00686FA0, object, nullptr, 0, eax);
            if (player == rosterId && nameRegistry != nullptr) {
                const std::uint32_t nameKey = 0x1F9;
                std::uint32_t looked = 0;
                const std::uint32_t lookupArgs[2] = {pointerValue(&looked), nameKey};
                const std::uint32_t handle =
                    services.callDirect(0x005B4D70, nameRegistry, lookupArgs, 2, eax);
                void* holder = pointerFrom(handle);
                void* text = holder == nullptr ? nullptr : pointerFrom(load32(holder, 0));
                if (holder != nullptr) {
                    store32(holder, 0, 0);
                }
                if (looked != 0) {
                    services.callDirect(0x005AF4D0, pointerFrom(looked), nullptr, 0, eax);
                    const std::uint32_t freed = looked;
                    services.callDirect(0x004166B0, nullptr, &freed, 1, eax);
                }
                const std::uint32_t path = load32(object, 0xDC);
                services.callDirect(0x005A8D00, text, &path, 1, eax);
                const std::uint32_t textValue = pointerValue(text);
                services.callDirect(0x005B4E70, nameRegistry, &textValue, 1, eax);
                services.callDirect(0x004D90E0, &looked, nullptr, 0, eax);
            }
        }
    }
    void** list = listAt(self, 0x178);
    void* element = *list;
    while (element != nullptr) {
        if ((raw(self)[0x13E] & 1) != 0 && raw(self)[0x258] == 5 && (raw(self)[0x13C] & 4) == 0) {
            const std::uint32_t produced = services.callSlot(element, 0xE4, nullptr, 0, eax);
            flag = (flag & 0xFFFFFF00u) | (produced & 0xFFu);
        }
        eax = services.callSlot(element, 0x4C, &flag, 1, eax);
        ++list;
        element = *list;
    }
    return eax;
}

std::uint32_t assignFlag13CBit1(
    void* self, std::uint32_t bit, DrawableServices& services, std::uint32_t eax)
{
    const std::uint8_t current = raw(self)[0x13C];
    const std::uint8_t argument = static_cast<std::uint8_t>(bit);
    eax = (eax & 0xFFFFFF00u) | argument;
    const std::uint8_t existing = static_cast<std::uint8_t>((current >> 1) & 1u);
    if (argument == existing) {
        return eax;
    }
    raw(self)[0x13C] = withBit1(current, argument);
    return refreshModuleGate(self, nullptr, nullptr, services, eax);
}

std::uint32_t refreshIfVisible(
    void* self, std::uint32_t argument, void* registry, DrawableServices& services,
    std::uint32_t eax)
{
    const std::uint8_t flags = raw(self)[0x13D];
    eax = (eax & 0xFFFFFF00u) | flags;
    if ((flags & 2) == 0 || (flags & 4) == 0 || (flags & 0x10) == 0) {
        return eax;
    }
    releaseTracked(self, registry, services, eax);
    void* object = pointerFrom(load32(self, 0x138));
    std::uint32_t produced = 0;
    if (object != nullptr) {
        void* child = pointerFrom(load32(object, 0x33C));
        if (child != nullptr) {
            produced = services.callSlot(child, 0x54, nullptr, 0, 0);
        }
    }
    return installResolved(self, produced, argument, registry, services, eax);
}

std::uint32_t assignFlag13DBit1(
    void* self, std::uint32_t bit, void* registry, DrawableServices& services,
    std::uint32_t eax)
{
    const std::uint8_t current = raw(self)[0x13D];
    eax = (eax & 0xFFFFFF00u) | current;
    const std::uint8_t argument = static_cast<std::uint8_t>(bit);
    if (static_cast<std::uint8_t>((current >> 1) & 1u) == argument) {
        return eax;
    }
    raw(self)[0x13D] = withBit1(current, argument);
    if (argument != 0) {
        return refreshIfVisible(self, 0, registry, services, eax);
    }
    return releaseTracked(self, registry, services, eax);
}

std::uint32_t applyAlternate(
    void* self, void* registry, DrawableServices& services, std::uint32_t eax)
{
    const std::uint8_t flags = raw(self)[0x13D];
    eax = (eax & 0xFFFFFF00u) | flags;
    if ((flags & 8) == 0 || (flags & 4) == 0 || (flags & 0x10) == 0) {
        return eax;
    }
    const std::uint32_t shifted = load32(self, 0x26C) >> 3;
    if ((shifted & 1u) == 0) {
        return shifted;
    }
    const std::uint32_t previous = load32(self, 0x210);
    eax = services.callSlot(registry, 0xD8, &previous, 1, eax);
    if ((eax & 0xFF) != 0) {
        return eax;
    }
    const std::uint32_t resolved = resolveRecord(self, 8, services);
    std::uint32_t local = 0;
    services.callDirect(0x0055A0D0, &local, &resolved, 1, eax);
    if (local == 0) {
        return services.callDirect(0x0055A140, &local, nullptr, 0, eax);
    }
    const std::uint32_t field = load32(self, 0x16C);
    const std::uint32_t args[2] = {pointerValue(&local), field};
    services.callDirect(0x0055D1A0, &local, args, 2, eax);
    eax = services.callSlot(registry, 0x64, &local, 1, eax);
    store32(self, 0x210, eax);
    services.callDirect(0x0055B530, &local, nullptr, 0, eax);
    return services.callDirect(0x0055A140, &local, nullptr, 0, eax);
}

std::uint32_t installResolved(
    void* self, std::uint32_t mode, std::uint32_t flag, void* registry,
    DrawableServices& services, std::uint32_t eax)
{
    eax = releaseTracked(self, registry, services, eax);
    std::uint32_t local = 0;
    const std::uint32_t lookupArgs[2] = {pointerValue(&local), mode};
    services.callDirect(0x0052D830, self, lookupArgs, 2, eax);
    if (local != 0) {
        if (flag != 0) {
            eax = services.callDirect(0x00559E60, pointerFrom(local), nullptr, 0, eax);
            if ((eax & 0xFF) == 0) {
                raw(self)[0x13D] = static_cast<std::uint8_t>(raw(self)[0x13D] & 0xDF);
                return services.callDirect(0x0055A140, &local, nullptr, 0, eax);
            }
        }
        const std::uint32_t field = load32(self, 0x16C);
        const std::uint32_t args[2] = {pointerValue(&local), field};
        services.callDirect(0x0055D1A0, &local, args, 2, eax);
        eax = services.callSlot(registry, 0x64, &local, 1, eax);
        store32(self, 0x20C, eax);
        services.callDirect(0x0055A0F0, raw(self) + 0x1F4, &local, 1, eax);
        services.callDirect(0x0055B530, &local, nullptr, 0, eax);
    }
    raw(self)[0x13D] = static_cast<std::uint8_t>(raw(self)[0x13D] & 0xDF);
    return services.callDirect(0x0055A140, &local, nullptr, 0, eax);
}

std::uint32_t setAttachedAndRefresh(
    void* self, void* registry, DrawableServices& services, std::uint32_t eax)
{
    const std::uint8_t flags = raw(self)[0x13D];
    eax = (eax & 0xFFFFFF00u) | flags;
    if ((flags & 0x10) != 0) {
        return eax;
    }
    raw(self)[0x13D] = static_cast<std::uint8_t>(flags | 0x10);
    eax = applyAlternate(self, registry, services, eax);
    const std::uint8_t updated = raw(self)[0x13D];
    if ((updated & 2) != 0 && (updated & 4) != 0 && (updated & 0x10) != 0) {
        releaseTracked(self, registry, services, eax);
        void* object = pointerFrom(load32(self, 0x138));
        std::uint32_t produced = 0;
        if (object != nullptr) {
            void* child = pointerFrom(load32(object, 0x33C));
            if (child != nullptr) {
                produced = services.callSlot(child, 0x54, nullptr, 0, 0);
            }
        }
        eax = installResolved(self, produced, 0, registry, services, eax);
    }
    void** modules = listAt(self, 0x180);
    if (modules != nullptr) {
        void* element = *modules;
        while (element != nullptr) {
            eax = services.callSlot(element, 0x34, nullptr, 0, eax);
            if (eax != 0) {
                eax = services.callSlot(pointerFrom(eax), 0, nullptr, 0, eax);
            }
            ++modules;
            element = *modules;
        }
    }
    void** clients = listAt(self, 0x17C);
    if (clients == nullptr) {
        return eax;
    }
    void* element = *clients;
    while (element != nullptr) {
        eax = services.callSlot(element, 0x3C, nullptr, 0, eax);
        if (eax != 0) {
            eax = services.callSlot(pointerFrom(eax), 0, nullptr, 0, eax);
        }
        ++clients;
        element = *clients;
    }
    return eax;
}

std::uint32_t refreshSupplied(
    void* self, std::uint32_t identifier, void* registry, DrawableServices& services,
    std::uint32_t eax)
{
    const std::uint8_t flags = raw(self)[0x13D];
    eax = (eax & 0xFFFFFF00u) | flags;
    if ((flags & 2) == 0 || (flags & 4) == 0 || (flags & 0x10) == 0) {
        return eax;
    }
    std::uint32_t local = identifier;
    const std::uint32_t args[2] = {pointerValue(&local), identifier};
    services.callDirect(0x0052D830, self, args, 2, eax);
    if (local != load32(self, 0x1F4)) {
        installResolved(self, identifier, 0, registry, services, eax);
    }
    return services.callDirect(0x0055A140, &local, nullptr, 0, eax);
}

std::uint32_t syncSecondary(void* secondary, DrawableServices& services, std::uint32_t eax)
{
    void* primary = raw(secondary) - 0x60;
    void* object = pointerFrom(load32(secondary, 0xD8));
    eax = pointerValue(object);
    if (object != nullptr) {
        const std::uint32_t source = pointerValue(raw(object) + 8);
        eax = services.callDirect(0x0078C500, primary, &source, 1, eax);
        if ((raw(secondary)[0xDE] & 4) != 0) {
            store32(secondary, 0xC4, load32(raw(secondary) - 0x28, 0));
            store32(secondary, 0xC8, load32(raw(secondary) - 0x24, 0));
            store32(secondary, 0xCC, load32(raw(secondary) - 0x20, 0));
            eax = copyTwelveDwords(raw(secondary) + 0x94, raw(secondary) - 0x58);
        }
    }
    eax = publishCondition(primary, services, eax);
    if (load32(secondary, 0x1E4) != load32(secondary, 0x1E8)) {
        eax = walkSlot(listAt(secondary, 0x118), 0xF0, nullptr, 0, services, eax);
    }
    return eax;
}

std::uint32_t updateSwayState(
    void* self, void* record, void* out, std::uint32_t logicFrame, DrawableServices& services)
{
    if (load32(self, 0x208) == 0) {
        void* created = services.allocate(0x70);
        if (created != nullptr) {
            constructSwayState(created, services);
        }
        store32(self, 0x208, pointerValue(created));
    }
    void* inner = pointerFrom(load32(pointerFrom(load32(record, 4)), 4));
    void* object = pointerFrom(load32(self, 0x138));
    if (object == nullptr || load32(object, 0x374) == 0) {
        return pointerValue(inner);
    }
    const float limit = loadFloat(inner, 0x88);
    const float gain = loadFloat(inner, 0x94);
    const float drag = loadFloat(inner, 0x9C);
    const float blend = loadFloat(inner, 0xC0);
    if (logicFrame > load32(self, 0x390)) {
        const float edgeX = loadFloat(self, 0xD0) - loadFloat(self, 0xC4);
        const float edgeY = loadFloat(self, 0xD4) - loadFloat(self, 0xC8);
        const float nextX = loadFloat(self, 0xDC) - loadFloat(self, 0xD0);
        const float nextY = loadFloat(self, 0xE0) - loadFloat(self, 0xD4);
        void* sway = pointerFrom(load32(self, 0x208));
        storeFloat(sway, 0xC, clampCross((edgeY * nextX - nextY * edgeX) * loadFloat(inner, 0xB4)));
        store32(self, 0x390, logicFrame);
    }
    void* sway = pointerFrom(load32(self, 0x208));
    __m128 velocity = _mm_load_ss(reinterpret_cast<float*>(raw(sway) + 0x14));
    __m128 delta = _mm_sub_ss(
        _mm_load_ss(reinterpret_cast<float*>(raw(sway) + 0xC)),
        _mm_load_ss(reinterpret_cast<float*>(raw(sway) + 0x10)));
    delta = _mm_sub_ss(_mm_mul_ss(delta, _mm_set_ss(gain)), _mm_mul_ss(velocity, _mm_set_ss(drag)));
    velocity = _mm_add_ss(delta, velocity);
    _mm_store_ss(reinterpret_cast<float*>(raw(sway) + 0x14), velocity);
    __m128 position = _mm_add_ss(
        _mm_mul_ss(velocity, _mm_set_ss(blend)),
        _mm_load_ss(reinterpret_cast<float*>(raw(sway) + 0x10)));
    _mm_store_ss(reinterpret_cast<float*>(raw(sway) + 0x10), position);
    storeFloat(out, 4, loadFloat(sway, 0x10));
    storeFloat(sway, 4, 0.f);
    storeFloat(sway, 0x1C, clampSymmetric(loadFloat(sway, 0x1C), limit));
    const float heading = loadFloat(sway, 0x24);
    storeFloat(out, 0xC, 0.f);
    if (heading > limit) {
        storeFloat(sway, 0x24, limit);
        return pointerValue(sway);
    }
    storeFloat(sway, 0x24, clampSymmetric(heading, limit));
    return pointerValue(sway);
}

}  // namespace ra3::gameclient
