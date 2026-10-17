#include <ra3/gameclient/ParticleModules66.h>

#include <immintrin.h>
#include <intrin.h>

#include <cstring>

namespace ra3::gameclient {
namespace {

void* loadPtr(const std::uint8_t* base, std::size_t offset) noexcept {
    void* value = nullptr;
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}

float sseAdd(float a, float b) noexcept {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

float sseSub(float a, float b) noexcept {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

float sseMul(float a, float b) noexcept {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

void x87CopyFloat(float& dst, const float& src) noexcept {
    const float* s = &src;
    float* d = &dst;
    __asm {
        mov eax, s
        mov ecx, d
        fld dword ptr [eax]
        fstp dword ptr [ecx]
    }
}

std::uint32_t x87Bits(const float& src) noexcept {
    float tmp = 0.0f;
    x87CopyFloat(tmp, src);
    std::uint32_t bits = 0;
    std::memcpy(&bits, &tmp, sizeof(bits));
    return bits;
}

std::int32_t mulHi(std::int32_t value, std::int32_t magic) noexcept {
    const auto product = static_cast<std::int64_t>(value) * static_cast<std::int64_t>(magic);
    return static_cast<std::int32_t>(product >> 32);
}

using LineFn = void (__cdecl*)(
    void* drawer, const float* a, const float* b, std::uint32_t color, std::uint32_t zero, std::uint32_t one);
using CircleFn = void (__cdecl*)(
    void* drawer, const float* point, std::uint32_t radiusBits, std::uint32_t color, std::uint32_t zero,
    std::uint32_t one);

void* drawer() noexcept {
    return g_fx66DebugRoot->drawer;
}

void** drawerVtable() noexcept {
    return *reinterpret_cast<void***>(drawer());
}

void drawLine(int slot, const float* a, const float* b) noexcept {
    auto fn = reinterpret_cast<LineFn>(drawerVtable()[slot]);
    fn(drawer(), a, b, kDebugDrawColor, 0, 1);
}

void drawCircle(const float* point, std::uint32_t radiusBits) noexcept {
    auto fn = reinterpret_cast<CircleFn>(drawerVtable()[0x44 / 4]);
    fn(drawer(), point, radiusBits, kDebugDrawColor, 0, 1);
}

void addRef(void* object) noexcept {
    auto* count = reinterpret_cast<volatile long*>(static_cast<std::uint8_t*>(object) + 4);
    _InterlockedExchangeAdd(count, 1);
}

long releaseCount(void* object) noexcept {
    auto* count = reinterpret_cast<volatile long*>(static_cast<std::uint8_t*>(object) + 4);
    return _InterlockedExchangeAdd(count, -1);
}

void destroyWithSlot0(void* object) noexcept {
    auto* count = reinterpret_cast<volatile long*>(static_cast<std::uint8_t*>(object) + 4);
    _InterlockedExchange(count, 1);
    auto fn = reinterpret_cast<void (__cdecl*)(void*, int)>(
        (*reinterpret_cast<void***>(object))[0]);
    fn(object, 1);
}

std::uint32_t* cacheNameKey(std::uint32_t& guard, std::uint32_t& slot, std::uint32_t pointer) noexcept {
    if ((guard & 1u) == 0u) {
        guard |= 1u;
        slot = pointer;
    }
    return &slot;
}

const std::uint8_t* nestedDrawData(const std::uint8_t* module) noexcept {
    const auto* first = *reinterpret_cast<const std::uint8_t* const*>(module + 4);
    const auto* second = *reinterpret_cast<const std::uint8_t* const*>(first);
    return *reinterpret_cast<const std::uint8_t* const*>(second + 8);
}

using XferOne = void (__cdecl*)(void* self, void* dest);
using XferNamed = void (__cdecl*)(void* self, const void* name, void* dest, int size);
using ModuleXfer = void (__cdecl*)(void* self, void* xfer);

void callXfer(void* xfer, int slot, void* dest) noexcept {
    auto fn = reinterpret_cast<XferOne>((*reinterpret_cast<void***>(xfer))[slot]);
    fn(xfer, dest);
}

void xferModule(void* module, void* xfer) noexcept {
    if (module == nullptr) {
        return;
    }
    auto fn = reinterpret_cast<ModuleXfer>((*reinterpret_cast<void***>(module))[3]);
    fn(module, xfer);
}

bool xferVersionSkips(void* xfer) noexcept {
    const auto version = *reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(xfer) + 4);
    return ((version >> 3) & 1u) != 0u;
}

}  // namespace

Fx66DebugRoot* g_fx66DebugRoot = nullptr;
Fx66AllocFn g_fx66Alloc = nullptr;
Fx66FreeFn g_fx66Free = nullptr;
Fx66Thiscall0 g_fx66SwarmPrepare = nullptr;
std::uint8_t g_fx66SwarmArmed = 0;
void* g_fx66SwarmHeld = nullptr;
std::uint32_t g_fx66SwarmPassGate = 0;
void* g_fx66TrailGate = nullptr;
std::uint8_t g_fx66TrailArmed = 0;
void* g_fx66Scene = nullptr;
void* g_fx66StreakSource = nullptr;
std::int32_t g_fx66AuxSentinelRef = 0;

void debugDrawBoxEmissionVolume(const std::uint8_t* module, const float* point) noexcept {
    const auto* tmpl = *reinterpret_cast<const std::uint8_t* const*>(module + 8);
    const float hx = *reinterpret_cast<const float*>(tmpl + 0x0C);
    const float hy = *reinterpret_cast<const float*>(tmpl + 0x10);
    const float hz = *reinterpret_cast<const float*>(tmpl + 0x14);
    const float px = point[0];
    const float py = point[1];
    const float pz = point[2];
    const float xs[4] = {sseAdd(px, hx), sseSub(px, hx), sseSub(px, hx), sseAdd(px, hx)};
    const float ys[4] = {sseAdd(py, hy), sseAdd(py, hy), sseSub(py, hy), sseSub(py, hy)};
    for (int i = 0; i < 4; ++i) {
        float high[3] = {xs[i], ys[i], sseAdd(pz, hz)};
        float low[3] = {xs[i], ys[i], sseSub(pz, hz)};
        drawLine(0x30 / 4, high, low);
    }
}

void debugDrawCylinderEmissionVolume(const std::uint8_t* module, const float* point) noexcept {
    const auto* tmpl = *reinterpret_cast<const std::uint8_t* const*>(module + 8);
    const float length = *reinterpret_cast<const float*>(tmpl + 0x14);
    const float half = sseMul(length, 0.5f);
    const float lowZ = sseSub(point[2], half);
    const float highZ = sseAdd(length, lowZ);
    const auto radiusBits = x87Bits(*reinterpret_cast<const float*>(module + 0x0C));
    float low[3] = {point[0], point[1], lowZ};
    float high[3] = {point[0], point[1], highZ};
    drawCircle(low, radiusBits);
    drawCircle(high, radiusBits);
}

void debugDrawLineEmissionVolume(const std::uint8_t* module, const float* point) noexcept {
    const auto* tmpl = *reinterpret_cast<const std::uint8_t* const*>(module + 8);
    const auto* start = reinterpret_cast<const float*>(tmpl + 0x0C);
    const auto* end = reinterpret_cast<const float*>(tmpl + 0x18);
    float a[3] = {sseAdd(point[0], start[0]), sseAdd(point[1], start[1]), sseAdd(point[2], start[2])};
    float b[3] = {sseAdd(point[0], end[0]), sseAdd(point[1], end[1]), sseAdd(point[2], end[2])};
    drawLine(0x30 / 4, a, b);
}

void debugDrawPointEmissionVolume(const float* point) noexcept {
    float a[3] = {point[0], point[1], point[2]};
    float b[3] = {point[0], point[1], sseAdd(point[2], 2.0f)};
    drawLine(0x30 / 4, a, b);
}

void debugDrawSphereEmissionVolume(const std::uint8_t* module, const float* point) noexcept {
    const auto* tmpl = *reinterpret_cast<const std::uint8_t* const*>(module + 8);
    const float radius = *reinterpret_cast<const float*>(tmpl + 0x0C);
    const float half = sseMul(radius, 0.5f);
    const float lowZ = sseSub(point[2], half);
    const float highZ = sseAdd(radius, lowZ);
    const auto radiusBits = x87Bits(radius);
    float low[3] = {point[0], point[1], lowZ};
    float high[3] = {point[0], point[1], highZ};
    drawCircle(low, radiusBits);
    drawCircle(high, radiusBits);
}

void debugDrawSplineEmissionVolume(const std::uint8_t* module, const float* point) noexcept {
    if (module[0x0C] == 0) {
        debugDrawLineEmissionVolume(module, point);
        return;
    }
    const auto* begin = *reinterpret_cast<const std::uint8_t* const*>(module + 0x10);
    const auto* end = *reinterpret_cast<const std::uint8_t* const*>(module + 0x14);
    const auto bytes = static_cast<std::int32_t>(end - begin);
    auto hi = mulHi(bytes, 0x2AAAAAAB);
    hi >>= 2;
    const auto quot = hi + static_cast<std::int32_t>(static_cast<std::uint32_t>(hi) >> 31);
    const auto segments = quot - 1;
    if (segments <= 0) {
        return;
    }
    for (std::int32_t i = 0; i < segments; ++i) {
        const auto* current = begin + static_cast<std::uint32_t>(i) * 0x18u;
        drawLine(0x30 / 4, reinterpret_cast<const float*>(current), reinterpret_cast<const float*>(current + 0x18));
    }
}

void debugDrawLightningEmissionVolume(const float* point) noexcept {
    float local[3] = {5.0f, 5.0f, 5.0f};
    drawLine(0x3C / 4, point, local);
}

void integrateSwarmStorageRecords(std::uint8_t* module) noexcept {
    auto* slot = *reinterpret_cast<std::uint8_t**>(module + 4);
    auto* system = *reinterpret_cast<std::uint8_t**>(slot);
    auto* rateObject = *reinterpret_cast<std::uint8_t**>(system + 0x20);
    if (rateObject == nullptr) {
        return;
    }
    auto viewFn = reinterpret_cast<void* (__cdecl*)(void*)>(
        (*reinterpret_cast<void***>(rateObject))[0x54 / 4]);
    auto* view = static_cast<std::uint8_t*>(viewFn(rateObject));
    if (view == nullptr) {
        return;
    }
    const float targetX = *reinterpret_cast<float*>(system + 0xE0);
    const float targetY = *reinterpret_cast<float*>(system + 0xF0);
    const float targetZ = *reinterpret_cast<float*>(system + 0x100);
    const auto* rateData = *reinterpret_cast<std::uint8_t**>(view + 8);
    const float rate = *reinterpret_cast<const float*>(rateData + 0x10);
    auto* cursor = *reinterpret_cast<std::uint8_t**>(module + 8);
    while (cursor != *reinterpret_cast<std::uint8_t**>(module + 0x0C)) {
        auto& life = *reinterpret_cast<std::int32_t*>(cursor + 0x18);
        life += -1;
        if (life == 0) {
            auto* end = *reinterpret_cast<std::uint8_t**>(module + 0x0C);
            auto* last = end - 0x1C;
            std::uint32_t words[7];
            std::memcpy(words, last, sizeof(words));
            std::memcpy(cursor, words, sizeof(words));
            *reinterpret_cast<std::uint8_t**>(module + 0x0C) = last;
            continue;
        }
        float& x = *reinterpret_cast<float*>(cursor + 0x00);
        float& y = *reinterpret_cast<float*>(cursor + 0x04);
        float& z = *reinterpret_cast<float*>(cursor + 0x08);
        float& vx = *reinterpret_cast<float*>(cursor + 0x0C);
        float& vy = *reinterpret_cast<float*>(cursor + 0x10);
        float& vz = *reinterpret_cast<float*>(cursor + 0x14);
        const float nvx = sseAdd(vx, sseMul(sseSub(targetX, x), rate));
        const float nvy = sseAdd(vy, sseMul(sseSub(targetY, y), rate));
        const float nvz = sseAdd(vz, sseMul(sseSub(targetZ, z), rate));
        vx = nvx;
        vy = nvy;
        vz = nvz;
        x = sseAdd(x, nvx);
        y = sseAdd(y, nvy);
        z = sseAdd(z, nvz);
        cursor += 0x1C;
    }
}

std::int32_t swarmStorageRecordCount(const std::uint8_t* module) noexcept {
    const auto* end = *reinterpret_cast<const std::uint8_t* const*>(module + 0x0C);
    const auto* begin = *reinterpret_cast<const std::uint8_t* const*>(module + 8);
    const auto bytes = static_cast<std::int32_t>(end - begin);
    auto hi = mulHi(bytes, static_cast<std::int32_t>(0x92492493u));
    hi += bytes;
    hi >>= 4;
    return hi + static_cast<std::int32_t>(static_cast<std::uint32_t>(hi) >> 31);
}

void swarmDrawAddRefCachedName(const std::uint8_t* module, void** out) noexcept {
    auto* name = loadPtr(static_cast<const std::uint8_t*>(loadPtr(module, 8)), 0x1C);
    *out = name;
    if (name != nullptr) {
        addRef(name);
    }
}

void gpuDrawAddRefCachedName(const std::uint8_t* module, void** out) noexcept {
    auto* name = loadPtr(static_cast<const std::uint8_t*>(loadPtr(module, 8)), 0x48);
    *out = name;
    if (name != nullptr) {
        addRef(name);
    }
}

void streakTemplateAddRefGlobalName(void** out) noexcept {
    auto* name = *reinterpret_cast<void**>(static_cast<std::uint8_t*>(g_fx66StreakSource) + 0x1C);
    *out = name;
    if (name != nullptr) {
        addRef(name);
    }
}

void trailTemplateAddRefGlobalName(void** out) noexcept {
    auto* name = *reinterpret_cast<void**>(static_cast<std::uint8_t*>(g_fx66TrailGate) + 0x1C);
    *out = name;
    if (name != nullptr) {
        addRef(name);
    }
}

void assignFxParticleAuxRef(std::uint8_t* particle, void** replacement) noexcept {
    auto* incoming = *replacement;
    addRef(incoming);
    auto* previous = *reinterpret_cast<void**>(particle + 0x44);
    if (releaseCount(previous) == 1 && previous != nullptr) {
        g_fx66Free(previous);
    }
    *reinterpret_cast<void**>(particle + 0x44) = incoming;
}

void resetFxParticleAuxRef(std::uint8_t* particle) noexcept {
    auto* current = *reinterpret_cast<std::uint8_t**>(particle + 0x44);
    if (reinterpret_cast<std::uintptr_t>(current) == kFxParticleAuxSentinel) {
        return;
    }
    if (*reinterpret_cast<std::uint32_t*>(current) == 0u) {
        return;
    }
    _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&g_fx66AuxSentinelRef), 1);
    if (releaseCount(current) == 1) {
        g_fx66Free(current);
    }
    *reinterpret_cast<std::uintptr_t*>(particle + 0x44) = kFxParticleAuxSentinel;
}

void releaseSwarmDrawGlobal() noexcept {
    if (g_fx66SwarmArmed == 0) {
        return;
    }
    auto* held = g_fx66SwarmHeld;
    g_fx66SwarmArmed = 0;
    if (held == nullptr) {
        return;
    }
    if (releaseCount(held) == 1) {
        destroyWithSlot0(held);
    }
    g_fx66SwarmHeld = nullptr;
}

void* exchangeGpuTemplateName(const std::uint8_t* tmpl, void** out) noexcept {
    auto* object = *reinterpret_cast<void**>(const_cast<std::uint8_t*>(tmpl + 0x44));
    if (object == nullptr) {
        *out = nullptr;
        return out;
    }
    addRef(object);
    auto* name = *reinterpret_cast<void**>(static_cast<std::uint8_t*>(object) + 0x1C);
    *out = name;
    if (name != nullptr) {
        addRef(name);
    }
    if (releaseCount(object) == 1) {
        destroyWithSlot0(object);
    }
    return out;
}

void copySplineEmissionTangent(const std::uint8_t* module, float* out) noexcept {
    x87CopyFloat(out[0], *reinterpret_cast<const float*>(module + 0x20));
    x87CopyFloat(out[1], *reinterpret_cast<const float*>(module + 0x24));
    x87CopyFloat(out[2], *reinterpret_cast<const float*>(module + 0x28));
}

void copyDrawModuleShaderPair(const std::uint8_t* module, std::uint32_t* out) noexcept {
    const auto* data = nestedDrawData(module);
    out[0] = *reinterpret_cast<const std::uint32_t*>(data + 0x5C);
    out[1] = *reinterpret_cast<const std::uint32_t*>(data + 0x60);
}

std::uint8_t* drawModulePayload(const std::uint8_t* module) noexcept {
    return const_cast<std::uint8_t*>(nestedDrawData(module)) + 0x54;
}

std::uint32_t drawModuleShaderWord(const std::uint8_t* module) noexcept {
    return *reinterpret_cast<const std::uint32_t*>(nestedDrawData(module) + 0x4C);
}

void xferFxParticleCore(std::uint8_t* particle, void* xfer) noexcept {
    if (xferVersionSkips(xfer)) {
        return;
    }
    std::uint8_t flags[2] = {1, 1};
    callXfer(xfer, 0x18 / 4, flags);
    callXfer(xfer, 0x58 / 4, particle + 4);
    callXfer(xfer, 0x58 / 4, particle + 0x10);
    callXfer(xfer, 0x58 / 4, particle + 0x1C);
    callXfer(xfer, 0x6C / 4, particle + 0x28);
    callXfer(xfer, 0x84 / 4, particle + 0x30);
}

void xferFxParticleExtended(std::uint8_t* particle, void* xfer) noexcept {
    if (xferVersionSkips(xfer)) {
        return;
    }
    std::uint8_t flags[2] = {1, 1};
    callXfer(xfer, 0x18 / 4, flags);
    callXfer(xfer, 0x58 / 4, particle + 0x70);
    callXfer(xfer, 0x58 / 4, particle + 4);
    callXfer(xfer, 0x58 / 4, particle + 0x10);
    callXfer(xfer, 0x58 / 4, particle + 0x1C);
    callXfer(xfer, 0x6C / 4, particle + 0x28);
    callXfer(xfer, 0x84 / 4, particle + 0x30);
    xferModule(*reinterpret_cast<void**>(particle + 0x4C), xfer);
    xferModule(*reinterpret_cast<void**>(particle + 0x50), xfer);
    xferModule(*reinterpret_cast<void**>(particle + 0x54), xfer);
    xferModule(*reinterpret_cast<void**>(particle + 0x5C), xfer);
    xferModule(*reinterpret_cast<void**>(particle + 0x58), xfer);
    auto* cursor = *reinterpret_cast<std::uint8_t**>(particle + 0x60);
    auto* end = *reinterpret_cast<std::uint8_t**>(particle + 0x64);
    while (cursor != end) {
        xferModule(*reinterpret_cast<void**>(cursor), xfer);
        cursor += 4;
    }
    callXfer(xfer, 0x6C / 4, particle + 0x7C);
    callXfer(xfer, 0x6C / 4, particle + 0x38);
    callXfer(xfer, 0x6C / 4, particle + 0x2C);
    std::uint32_t particleId = 0;
    auto* aux = *reinterpret_cast<std::uint8_t**>(particle + 0x44);
    if (reinterpret_cast<std::uintptr_t>(aux) != kFxParticleAuxSentinel &&
        *reinterpret_cast<std::uint32_t*>(aux) != 0u) {
        auto* auxVtable = *reinterpret_cast<std::uint8_t**>(aux);
        particleId = *reinterpret_cast<std::uint32_t*>(auxVtable + 0x94);
    }
    auto named = reinterpret_cast<XferNamed>((*reinterpret_cast<void***>(xfer))[0x88 / 4]);
    named(xfer, reinterpret_cast<const void*>(0x00BE1CDCu), &particleId, 4);
}

void* createDefaultPhysicsParticleModule(
    const std::uint8_t* module, void* systemArg, sim::RandomState& rng) noexcept {
    auto* block = static_cast<std::uint8_t*>(g_fx66Alloc(0x10));
    if (block == nullptr) {
        return nullptr;
    }
    *reinterpret_cast<void**>(block + 4) = systemArg;
    *reinterpret_cast<std::uint32_t*>(block) = 0x00BF2B44u;
    const auto* tmpl = *reinterpret_cast<const std::uint8_t* const*>(module + 8);
    const float sample = sim::clientRandomVariableExtended(
        reinterpret_cast<const sim::RandomVariable*>(tmpl + 0x14), &rng);
    *reinterpret_cast<float*>(block + 8) = sample;
    block[0x0C] = tmpl[0x20];
    block[0x0D] = tmpl[0x21];
    return block;
}

void* createTerrainCollisionModule(
    std::uint8_t* tmpl, void** systemSlot, sim::RandomState& rng) noexcept {
    auto* block = static_cast<std::uint8_t*>(g_fx66Alloc(0x14));
    if (block == nullptr) {
        return nullptr;
    }
    auto* system = *systemSlot;
    *reinterpret_cast<void**>(block + 4) = system;
    addRef(system);
    *reinterpret_cast<void**>(block + 8) = tmpl;
    *reinterpret_cast<std::uint32_t*>(block) = 0x00BF4648u;
    const float sample = sim::clientRandomVariableExtended(
        reinterpret_cast<const sim::RandomVariable*>(tmpl + 0x14), &rng);
    *reinterpret_cast<float*>(block + 0x0C) = sample;
    block[0x10] = 1;
    return block;
}

void* cloneBoxEmissionVolumeTemplate(const std::uint8_t* tmpl) noexcept {
    auto* block = static_cast<std::uint8_t*>(g_fx66Alloc(0x18));
    if (block == nullptr) {
        return nullptr;
    }
    *reinterpret_cast<std::uint32_t*>(block + 4) = *reinterpret_cast<const std::uint32_t*>(tmpl + 4);
    block[8] = tmpl[8];
    *reinterpret_cast<std::uint32_t*>(block) = 0x00BF35D0u;
    x87CopyFloat(*reinterpret_cast<float*>(block + 0x0C), *reinterpret_cast<const float*>(tmpl + 0x0C));
    x87CopyFloat(*reinterpret_cast<float*>(block + 0x10), *reinterpret_cast<const float*>(tmpl + 0x10));
    x87CopyFloat(*reinterpret_cast<float*>(block + 0x14), *reinterpret_cast<const float*>(tmpl + 0x14));
    return block;
}

void* cloneDefaultPhysicsTemplate(const std::uint8_t* tmpl) noexcept {
    auto* block = static_cast<std::uint8_t*>(g_fx66Alloc(0x24));
    if (block == nullptr) {
        return nullptr;
    }
    *reinterpret_cast<std::uint32_t*>(block) = 0x00BF35B8u;
    x87CopyFloat(*reinterpret_cast<float*>(block + 4), *reinterpret_cast<const float*>(tmpl + 4));
    x87CopyFloat(*reinterpret_cast<float*>(block + 8), *reinterpret_cast<const float*>(tmpl + 8));
    x87CopyFloat(*reinterpret_cast<float*>(block + 0x0C), *reinterpret_cast<const float*>(tmpl + 0x0C));
    x87CopyFloat(*reinterpret_cast<float*>(block + 0x10), *reinterpret_cast<const float*>(tmpl + 0x10));
    *reinterpret_cast<std::uint32_t*>(block + 0x14) = *reinterpret_cast<const std::uint32_t*>(tmpl + 0x14);
    *reinterpret_cast<std::uint32_t*>(block + 0x18) = *reinterpret_cast<const std::uint32_t*>(tmpl + 0x18);
    *reinterpret_cast<std::uint32_t*>(block + 0x1C) = *reinterpret_cast<const std::uint32_t*>(tmpl + 0x1C);
    block[0x20] = tmpl[0x20];
    block[0x21] = tmpl[0x21];
    return block;
}

void* cloneLifeEventTemplate(const std::uint8_t* tmpl) noexcept {
    auto* block = static_cast<std::uint8_t*>(g_fx66Alloc(0x20));
    if (block == nullptr) {
        return nullptr;
    }
    *reinterpret_cast<std::uint32_t*>(block + 4) = *reinterpret_cast<const std::uint32_t*>(tmpl + 4);
    block[8] = tmpl[8];
    block[9] = tmpl[9];
    *reinterpret_cast<std::uint32_t*>(block + 0x0C) = *reinterpret_cast<const std::uint32_t*>(tmpl + 0x0C);
    *reinterpret_cast<std::uint32_t*>(block + 0x10) = *reinterpret_cast<const std::uint32_t*>(tmpl + 0x10);
    *reinterpret_cast<std::uint32_t*>(block) = 0x00BF2ECCu;
    std::memcpy(block + 0x14, tmpl + 0x14, 12);
    return block;
}

void beginTrailDrawPass(const std::uint8_t* module) noexcept {
    if (g_fx66TrailGate == nullptr) {
        return;
    }
    auto* system = **reinterpret_cast<std::uint8_t* const* const*>(module + 4);
    auto* attachment = *reinterpret_cast<std::uint8_t**>(system + 0x90);
    auto query = reinterpret_cast<void* (__cdecl*)(void*)>(
        (*reinterpret_cast<void***>(attachment))[0x2C / 4]);
    if (query(attachment) == nullptr) {
        return;
    }
    g_fx66TrailArmed = 1;
}

void prepareSwarmDrawPass(const std::uint8_t* module, const std::uint8_t* arg) noexcept {
    if (*reinterpret_cast<const std::uint32_t*>(arg + 8) != 3u) {
        return;
    }
    auto* system = **reinterpret_cast<std::uint8_t* const* const*>(module + 4);
    const auto shader = *reinterpret_cast<const std::uint32_t*>(
        *reinterpret_cast<const std::uint8_t* const*>(system + 8) + 0x50);
    if (shader == 5u || shader == 6u) {
        return;
    }
    auto* name = loadPtr(static_cast<const std::uint8_t*>(loadPtr(module, 8)), 0x20);
    if (name != nullptr) {
        addRef(name);
        if (g_fx66SwarmPassGate != 0u) {
            g_fx66SwarmArmed = 1;
        }
        if (releaseCount(name) == 1) {
            destroyWithSlot0(name);
        }
    }
}

std::uint32_t submitSwarmDraw(std::uint8_t* module) noexcept {
    if (g_fx66SwarmArmed == 0) {
        return 0;
    }
    if (module[0x10] == 0) {
        auto* member = *reinterpret_cast<std::uint8_t**>(module + 0x0C);
        auto fn = *reinterpret_cast<void (__cdecl**)(void*)>(member + 4);
        fn(module + 0x0C);
        module[0x10] = 1;
    }
    if (*reinterpret_cast<std::uint32_t*>(module + 0x18) == 0u) {
        return 0;
    }
    g_fx66SwarmPrepare(module);
    const auto count = *reinterpret_cast<std::uint32_t*>(module + 0x14);
    if (count == 0u) {
        return 0;
    }
    auto* mesh = *reinterpret_cast<std::uint8_t**>(module + 0x1C);
    *reinterpret_cast<std::uint32_t*>(mesh + 0x34) = 0;
    *reinterpret_cast<std::uint32_t*>(mesh + 0x38) = count * 4u;
    *reinterpret_cast<std::uint32_t*>(mesh + 0x28) = 0;
    *reinterpret_cast<std::uint32_t*>(mesh + 0x2C) = 0;
    *reinterpret_cast<std::uint32_t*>(mesh + 0x30) = count * 2u;
    auto* scene = static_cast<std::uint8_t*>(g_fx66Scene);
    auto* self = *reinterpret_cast<void**>(scene + 0x58);
    auto submit = *reinterpret_cast<void (__cdecl**)(void*, void*, void*, void*, void*)>(scene + 0x18);
    submit(self, scene, self, mesh, g_fx66SwarmHeld);
    return *reinterpret_cast<std::uint32_t*>(module + 0x14);
}

std::uint32_t* cacheDefaultAlphaTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept {
    return cacheNameKey(guard, slot, 0x00BF3048u);
}

std::uint32_t* cacheDefaultColorTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept {
    return cacheNameKey(guard, slot, 0x00BF3050u);
}

std::uint32_t* cacheDefaultDrawTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept {
    return cacheNameKey(guard, slot, 0x00BF3088u);
}

std::uint32_t* cacheDefaultPhysicsTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept {
    return cacheNameKey(guard, slot, 0x00BF3060u);
}

std::uint32_t* cacheDefaultUpdateTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept {
    return cacheNameKey(guard, slot, 0x00BF3078u);
}

std::uint32_t* cacheGpuDrawTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept {
    return cacheNameKey(guard, slot, 0x00BF3090u);
}

std::uint32_t* cacheLifeEventTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept {
    return cacheNameKey(guard, slot, 0x00BF30D0u);
}

std::uint32_t* cacheLightningDrawTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept {
    return cacheNameKey(guard, slot, 0x00BF30A0u);
}

std::uint32_t* cacheLightSourceDrawTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept {
    return cacheNameKey(guard, slot, 0x00BF3098u);
}

std::uint32_t* cacheSwarmDrawTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept {
    return cacheNameKey(guard, slot, 0x00BF30B8u);
}

}  // namespace ra3::gameclient
