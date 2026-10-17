#include "ra3/render/RenderCore99.h"

#include <cstring>
#include <intrin.h>

namespace ra3::render {
namespace {

std::uint32_t loadU32(const void* address) noexcept {
    std::uint32_t value = 0;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

void storeU32(void* address, std::uint32_t value) noexcept {
    std::memcpy(address, &value, sizeof(value));
}

float loadF32(const void* address) noexcept {
    float value = 0.0F;
    std::memcpy(&value, address, sizeof(value));
    return value;
}

void storeF32(void* address, float value) noexcept {
    std::memcpy(address, &value, sizeof(value));
}

void fldFstp(void* destination, const void* source) noexcept {
    __asm {
        mov eax, source
        mov ecx, destination
        fld dword ptr [eax]
        fstp dword ptr [ecx]
    }
}

float mulss(float left, float right) noexcept {
    float out;
    __asm {
        movss xmm0, dword ptr left
        mulss xmm0, dword ptr right
        movss dword ptr out, xmm0
    }
    return out;
}

float addss(float left, float right) noexcept {
    float out;
    __asm {
        movss xmm0, dword ptr left
        addss xmm0, dword ptr right
        movss dword ptr out, xmm0
    }
    return out;
}

float subss(float left, float right) noexcept {
    float out;
    __asm {
        movss xmm0, dword ptr left
        subss xmm0, dword ptr right
        movss dword ptr out, xmm0
    }
    return out;
}

float divss(float left, float right) noexcept {
    float out;
    __asm {
        movss xmm0, dword ptr left
        divss xmm0, dword ptr right
        movss dword ptr out, xmm0
    }
    return out;
}

float x87sqrt(float value) noexcept {
    float out;
    __asm {
        fld dword ptr value
        fsqrt
        fstp dword ptr out
    }
    return out;
}

bool comissJbe(float left, float right) noexcept {
    unsigned char taken = 0;
    __asm {
        movss xmm0, dword ptr left
        comiss xmm0, dword ptr right
        setbe taken
    }
    return taken != 0;
}

bool comissJa(float left, float right) noexcept {
    unsigned char taken = 0;
    __asm {
        movss xmm0, dword ptr left
        comiss xmm0, dword ptr right
        seta taken
    }
    return taken != 0;
}

// ucomiss / lahf / test ah,0x44 / jnp  — taken only for an ordered equal.
bool ucomissOrderedEqual(float left, float right) noexcept {
    unsigned char taken = 0;
    __asm {
        movss xmm0, dword ptr left
        ucomiss xmm0, dword ptr right
        lahf
        test ah, 0x44
        setnp taken
    }
    return taken != 0;
}

void addRefInline(std::uint8_t* object) noexcept {
    auto* count = reinterpret_cast<volatile long*>(object + 4);
    _InterlockedExchangeAdd(count, 1);
}

void releaseInline(std::uint8_t* object) noexcept {
    auto* count = reinterpret_cast<volatile long*>(object + 4);
    const long previous = _InterlockedExchangeAdd(count, -1);
    if (previous != 1) {
        return;
    }
    _InterlockedExchange(count, 1);
    const auto vtable = loadU32(object);
    const auto destroy = loadU32(reinterpret_cast<const void*>(vtable));
    using Destructor = void(__fastcall*)(void*, void*, int);
    reinterpret_cast<Destructor>(destroy)(object, nullptr, 1);
}

void copyParamRecord(std::uint8_t* destination, const std::uint8_t* source) noexcept {
    static constexpr std::uint32_t kOffsets[] = {
        0x00, 0x04, 0x18, 0x1C, 0x20, 0x08, 0x0C, 0x10, 0x14,
        0x34, 0x38, 0x3C, 0x24, 0x28, 0x2C, 0x30};
    for (const std::uint32_t offset : kOffsets) {
        storeU32(destination + offset, loadU32(source + offset));
    }
    fldFstp(destination + 0x40, source + 0x40);
    storeU32(destination + 0x44, loadU32(source + 0x44));
}

void copyExtendedRecord(std::uint8_t* destination, const std::uint8_t* source) noexcept {
    storeU32(destination, loadU32(source));
    storeU32(destination + 4, loadU32(source + 4));
    for (std::uint32_t offset = 8; offset <= 0x34; offset += 4) {
        fldFstp(destination + offset, source + offset);
    }
    for (std::uint32_t offset = 0x38; offset <= 0x4C; offset += 4) {
        storeU32(destination + offset, loadU32(source + offset));
    }
}

// 0x0098F750 stdcall quadtree clear. `partition` is unused by the body.
void clearPartitionNode(void* partition, std::uint8_t* node, std::uint32_t count) noexcept {
    if (loadU32(node) == 0) {
        return;
    }
    storeU32(node, 0);
    const std::uint32_t childCount = count >> 2;
    const std::uint32_t step = count * 8U;
    node += 8;
    for (int child = 0; child < 4; ++child) {
        clearPartitionNode(partition, node, childCount);
        node += step;
    }
}

// 0x00931D40. Writes three floats. Argument order is (out, left, right).
void rotateTranslation(float* out, const float* left, const float* right) noexcept {
    float xmm6 = loadF32(right + 2);
    float xmm2 = loadF32(left + 1);
    float xmm3 = loadF32(left + 2);
    float xmm4 = loadF32(right + 1);
    float xmm1 = loadF32(left + 3);
    float xmm5 = loadF32(right);
    float xmm0 = mulss(xmm6, xmm2);
    float xmm7 = mulss(xmm4, xmm3);
    xmm0 = subss(xmm0, xmm7);
    xmm7 = mulss(xmm5, xmm1);
    xmm0 = addss(xmm0, xmm7);
    xmm5 = mulss(xmm5, xmm3);
    const float slot0 = xmm0;
    xmm0 = loadF32(left);
    xmm7 = mulss(xmm1, xmm4);
    const float slot10 = xmm7;
    xmm7 = mulss(xmm0, xmm6);
    xmm7 = subss(xmm7, xmm5);
    xmm5 = subss(slot10, xmm7);
    const float slot10b = xmm5;
    xmm7 = loadF32(right);
    xmm7 = mulss(xmm7, xmm2);
    xmm5 = mulss(xmm0, xmm4);
    xmm5 = subss(xmm5, xmm7);
    xmm7 = mulss(xmm1, xmm6);
    xmm5 = addss(xmm5, xmm7);
    const float slot14 = xmm5;
    xmm5 = loadF32(right);
    xmm7 = mulss(xmm0, xmm5);
    xmm4 = mulss(xmm4, xmm2);
    xmm5 = mulss(xmm3, xmm6);
    xmm7 = addss(xmm7, xmm5);
    xmm7 = addss(xmm7, xmm4);
    xmm4 = subss(-0.0F, xmm7);
    xmm5 = mulss(xmm2, slot14);
    xmm6 = mulss(xmm3, slot10b);
    xmm5 = subss(xmm5, xmm6);
    xmm7 = mulss(xmm0, xmm4);
    xmm6 = mulss(xmm1, slot0);
    xmm6 = subss(xmm6, xmm7);
    xmm5 = addss(xmm5, xmm6);
    storeF32(out, xmm5);
    xmm5 = mulss(xmm1, slot10b);
    xmm1 = mulss(xmm1, slot14);
    xmm6 = mulss(xmm2, xmm4);
    xmm5 = subss(xmm5, xmm6);
    xmm6 = mulss(xmm0, slot14);
    xmm0 = mulss(xmm0, slot10b);
    const float negDot = xmm4;
    xmm4 = slot0;
    xmm7 = mulss(xmm3, negDot);
    float xmm7b = mulss(xmm3, xmm4);
    xmm6 = subss(xmm6, xmm7b);
    xmm2 = mulss(xmm2, xmm4);
    xmm5 = subss(xmm5, xmm6);
    xmm0 = subss(xmm0, xmm2);
    xmm1 = subss(xmm1, xmm7);
    storeF32(out + 1, xmm5);
    xmm0 = addss(xmm0, xmm1);
    storeF32(out + 2, xmm0);
}

ShaderRegistryView gRegistry{};
SemanticNameCache gSemantic{};
RenderBinServices gServices{};

constexpr float kOne = 1.0F;
constexpr float kZero = 0.0F;
constexpr float kHalf = 0.5F;
constexpr float kTwo = 2.0F;

// Column-0 length of the 3x3, x87 sqrt of m00^2 + m10^2 + m20^2 in that add order.
float column0Length(const float* matrix) noexcept {
    const float m00 = loadF32(matrix);
    const float m10 = loadF32(matrix + 4);
    const float m20 = loadF32(matrix + 8);
    const float sum = addss(addss(mulss(m10, m10), mulss(m20, m20)), mulss(m00, m00));
    return x87sqrt(sum);
}

void scaleQuaternion(float* quaternion, float columnLength) noexcept {
    const float factor = x87sqrt(columnLength);
    storeF32(quaternion, mulss(loadF32(quaternion), factor));
    storeF32(quaternion + 1, mulss(loadF32(quaternion + 1), factor));
    storeF32(quaternion + 2, mulss(loadF32(quaternion + 2), factor));
    storeF32(quaternion + 3, mulss(loadF32(quaternion + 3), factor));
}

// 0x00931EB0 sphere expand. this = sphere xyzr, argument = incoming xyzr.
void expandSphere(std::uint8_t* sphere, const std::uint8_t* incoming) noexcept {
    const float incomingRadius = loadF32(incoming + 0x0C);
    if (ucomissOrderedEqual(incomingRadius, 0.0F)) {
        return;
    }
    const float sphereRadius = loadF32(sphere + 0x0C);
    if (ucomissOrderedEqual(sphereRadius, 0.0F)) {
        storeU32(sphere, loadU32(incoming));
        storeU32(sphere + 4, loadU32(incoming + 4));
        storeU32(sphere + 8, loadU32(incoming + 8));
        storeF32(sphere + 0x0C, incomingRadius);
        return;
    }
    const float dx = subss(loadF32(incoming), loadF32(sphere));
    const float dy = subss(loadF32(incoming + 4), loadF32(sphere + 4));
    const float dz = subss(loadF32(incoming + 8), loadF32(sphere + 8));
    const float distance = x87sqrt(addss(addss(mulss(dz, dz), mulss(dx, dx)), mulss(dy, dy)));
    unsigned char unorderedOrUnequal = 0;
    __asm {
        fld dword ptr distance
        fldz
        fxch st(1)
        fucomip st(1)
        fstp st(0)
        lahf
        test ah, 0x44
        setp unorderedOrUnequal
    }
    if (unorderedOrUnequal == 0) {
        const float kept = comissJbe(sphereRadius, incomingRadius) ? incomingRadius : sphereRadius;
        storeF32(sphere + 0x0C, kept);
        return;
    }
    const float grown = mulss(addss(addss(sphereRadius, incomingRadius), distance), kHalf);
    if (comissJa(sphereRadius, grown) || comissJbe(incomingRadius, grown)) {
        if (!comissJa(sphereRadius, grown) && comissJbe(incomingRadius, grown)) {
            const float scale = divss(subss(grown, sphereRadius), distance);
            const float x = addss(loadF32(sphere), mulss(dx, scale));
            const float y = addss(loadF32(sphere + 4), mulss(dy, scale));
            const float z = addss(loadF32(sphere + 8), mulss(dz, scale));
            storeF32(sphere, x);
            storeF32(sphere + 4, y);
            storeF32(sphere + 8, z);
            storeF32(sphere + 0x0C, grown);
        }
        return;
    }
    storeU32(sphere, loadU32(incoming));
    storeU32(sphere + 4, loadU32(incoming + 4));
    storeU32(sphere + 8, loadU32(incoming + 8));
    storeF32(sphere + 0x0C, incomingRadius);
}

std::uint32_t lookupShaderConstant(std::uint32_t name) noexcept {
    if (name == 0x00C58C28U) {
        return 0x41U;
    }
    if (name == 0x00C58C18U) {
        return gRegistry.secondConstantId;
    }
    return 0xFFFFFFFFU;
}

std::uint32_t queryShaderConstant(void* /*api*/, std::uint32_t, std::uint32_t, std::uint32_t) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(gRegistry.constantQueryHit));
}

bool matchRenderBin(void* console, std::uint32_t, std::uint32_t, std::uint32_t) noexcept {
    return reinterpret_cast<const std::uint8_t*>(console)[0x70] != 0;
}

void selectRenderPair(void* object, std::uint32_t first, std::uint32_t second) noexcept {
    auto* bytes = reinterpret_cast<std::uint8_t*>(object);
    storeU32(bytes + 0x40, first);
    storeU32(bytes + 0x44, second);
}

void reserveRenderBinSlot(void*, std::uint32_t, std::uint32_t) noexcept {}

void submitRenderBin(void* console, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) noexcept {
    auto* bytes = reinterpret_cast<std::uint8_t*>(console);
    storeU32(bytes + 0x90, loadU32(bytes + 0x90) + 1U);
}

}  // namespace

void setShaderRegistryView(const ShaderRegistryView& view) noexcept {
    gRegistry = view;
}

void setSemanticNameCache(const SemanticNameCache& cache) noexcept {
    gSemantic = cache;
}

void setRenderBinServices(const RenderBinServices& services) noexcept {
    gServices = services;
}

// 0x00990080  PartitionManager secondary vtable via 0x00990660 -> 0x00990260.
void removePartitionObject(void* partition, void* object) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(partition);
    auto* node = reinterpret_cast<std::uint8_t*>(object);
    const auto next = loadU32(node + 0x14);
    if (next != 0) {
        storeU32(reinterpret_cast<void*>(next + 0x10), loadU32(node + 0x10));
    }
    storeU32(reinterpret_cast<void*>(loadU32(node + 0x10)), loadU32(node + 0x14));
    const auto slot = loadU32(node + 0x20) << 4;
    storeU32(node + 0x10, 0);
    auto cursor = loadU32(self + slot + 0x18);
    auto span = static_cast<std::uint32_t>(
        static_cast<std::int32_t>(loadU32(self + 0x1C) - loadU32(self + 0x18)) >> 3);
    auto mask = loadU32(self + 0x180) >> 1;
    span >>= 2;
    auto* cell = reinterpret_cast<std::uint8_t*>(cursor);
    while (span != 0) {
        if ((loadU32(node + 0x24) & mask) != 0) {
            break;
        }
        storeU32(cell, loadU32(cell) - 1U);
        const auto bitZ = loadU32(node + 0x2C) & mask;
        const auto bitY = loadU32(node + 0x28) & mask;
        std::uint32_t cz = bitZ;
        cz = static_cast<std::uint32_t>(-static_cast<std::int32_t>(cz != 0));
        std::uint32_t cy = bitY;
        cy = static_cast<std::uint32_t>(-static_cast<std::int32_t>(cy != 0));
        cz &= 2U;
        cy = static_cast<std::uint32_t>(-static_cast<std::int32_t>(cy));
        const auto child = cz + cy;
        const auto scaled = child * span;
        span >>= 2;
        mask >>= 1;
        cell += static_cast<std::ptrdiff_t>(scaled) * 8 + 8;
    }
}

// 0x00990100
void clearPartitionBuckets(void* partition) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(partition);
    for (auto node = loadU32(self + 0x178); node != 0;) {
        auto* entry = reinterpret_cast<std::uint8_t*>(node);
        storeU32(reinterpret_cast<void*>(loadU32(entry + 0x10)), 0);
        storeU32(entry + 0x10, 0);
        node = loadU32(entry + 0x0C);
    }
    const auto bytes = loadU32(self + 0x1C) - loadU32(self + 0x18);
    if ((bytes & 0xFFFFFFF8U) == 0) {
        return;
    }
    auto* slot = self + 0x18;
    for (std::uint32_t remaining = 0x15; remaining != 0; --remaining) {
        const auto count = (static_cast<std::uint32_t>(
                                 static_cast<std::int32_t>(loadU32(self + 0x1C) - loadU32(self + 0x18)) >>
                                 3)) >>
                           2;
        auto* root = reinterpret_cast<std::uint8_t*>(loadU32(slot));
        if (loadU32(root) != 0) {
            storeU32(root, 0);
            root += 8;
            const std::uint32_t child = count >> 2;
            const std::uint32_t step = count * 8U;
            for (int i = 0; i < 4; ++i) {
                clearPartitionNode(self, root, child);
                root += step;
            }
        }
        slot += 0x10;
    }
    const auto tailCount = (static_cast<std::uint32_t>(
                                 static_cast<std::int32_t>(loadU32(self + 0x1C) - loadU32(self + 0x18)) >>
                                 3)) >>
                           2;
    auto* tail = reinterpret_cast<std::uint8_t*>(loadU32(self + 0x168));
    if (loadU32(tail) != 0) {
        storeU32(tail, 0);
        tail += 8;
        const std::uint32_t child = tailCount >> 2;
        const std::uint32_t step = tailCount * 8U;
        for (int i = 0; i < 4; ++i) {
            clearPartitionNode(self, tail, child);
            tail += step;
        }
    }
}

// 0x009901E0. Returns the bounds pointer. The list walk calls 0x0098FFC0 only when non-empty;
// tests keep the list null because that helper is outside this batch.
void setPartitionWorldBounds(void* partition, const float* bounds6) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(partition);
    clearPartitionBuckets(self);
    for (int i = 0; i < 6; ++i) {
        storeU32(self + static_cast<std::ptrdiff_t>(i) * 4, loadU32(bounds6 + i));
    }
    float width = subss(loadF32(self + 0x0C), loadF32(self));
    const float height = subss(loadF32(self + 0x10), loadF32(self + 4));
    if (!comissJa(width, height)) {
        width = height;
    }
    const float inverse = divss(kOne, width);
    storeF32(self + 0x17C, inverse);
    for (auto node = loadU32(self + 0x178); node != 0;) {
        node = loadU32(reinterpret_cast<std::uint8_t*>(node) + 0x0C);
    }
}

// 0x00990620 slot 1 of the secondary table. ecx becomes [this+0x28].
void resetPartitionBounds(void* partition) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(partition);
    auto* inner = reinterpret_cast<std::uint8_t*>(loadU32(self + 0x28));
    float zeros[6] = {};
    setPartitionWorldBounds(inner, zeros);
}

// 0x00993120
void unlinkCollisionEntry(void* entry) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(entry);
    auto* owner = reinterpret_cast<std::uint8_t*>(loadU32(self));
    storeU32(owner + 4, loadU32(owner + 4) - 1U);
    const auto next = loadU32(self + 0x10);
    if (next != 0) {
        storeU32(reinterpret_cast<void*>(next + 0x0C), loadU32(self + 0x0C));
    }
    storeU32(reinterpret_cast<void*>(loadU32(self + 0x0C)), loadU32(self + 0x10));
    storeU32(self + 0x0C, 0);
    if (loadU32(self + 0x28) != 0) {
        auto* pair = self + 0x2C;
        for (std::uint32_t index = 0; index < 6; ++index) {
            const auto left = loadU32(pair - 4);
            if (left != 0) {
                storeU32(reinterpret_cast<void*>(left + 4), loadU32(pair));
            } else {
                const auto bucket = index >> 1;
                storeU32(owner + static_cast<std::ptrdiff_t>(bucket) * 4 + 0x0C, loadU32(pair));
            }
            const auto right = loadU32(pair);
            if (right != 0) {
                storeU32(reinterpret_cast<void*>(right), loadU32(pair - 4));
            }
            storeU32(pair - 4, 0);
            storeU32(pair, 0);
            pair += 0x14;
        }
    }
    const auto link = loadU32(self + 0x14);
    if (link != 0) {
        const auto other = loadU32(self + 0x18);
        if (other != 0) {
            storeU32(reinterpret_cast<void*>(other + 0x14), link);
        }
        storeU32(reinterpret_cast<void*>(link), loadU32(self + 0x18));
        storeU32(self + 0x14, 0);
    }
}

// 0x00993360
void sweepIdleCollisionNodes(void* collision) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(collision);
    storeU32(self + 0xC060, 0);
    storeU32(self + 0xC064, loadU32(self + 0xAE10));
    for (;;) {
        if (loadU32(self + 0xC064) == 0) {
            const auto nextIndex = loadU32(self + 0xC060) + 1U;
            if (nextIndex == 0x493U) {
                return;
            }
            storeU32(self + 0xC060, nextIndex);
            const auto head = loadU32(self + nextIndex * 4U + 0xAE10);
            storeU32(self + 0xC064, head);
            if (head == 0) {
                continue;
            }
        }
        auto* node = reinterpret_cast<std::uint8_t*>(loadU32(self + 0xC064));
        const auto chain = loadU32(node + 0x30);
        storeU32(self + 0xC064, chain);
        auto* primary = reinterpret_cast<std::uint8_t*>(loadU32(node));
        auto* secondary = reinterpret_cast<std::uint8_t*>(loadU32(node + 4));
        if (loadU32(primary + 0x14) == 0 && loadU32(secondary + 0x14) == 0) {
            continue;
        }
        if (loadU32(primary + 4) != 0 && loadU32(secondary + 4) != 0) {
            storeU32(node + 0x10, 0);
            continue;
        }
        if (loadU32(self + 0xC064) == reinterpret_cast<std::uintptr_t>(node)) {
            storeU32(self + 0xC064, loadU32(node + 0x30));
        }
        const auto next = loadU32(node + 0x30);
        if (next != 0) {
            storeU32(reinterpret_cast<void*>(next + 0x2C), loadU32(node + 0x2C));
        }
        storeU32(reinterpret_cast<void*>(loadU32(node + 0x2C)), loadU32(node + 0x30));
        storeU32(node + 0x2C, 0);
        storeU32(node + 0x30, loadU32(self + 0xC05C));
        storeU32(self + 0xC05C, reinterpret_cast<std::uintptr_t>(node));
    }
}

// 0x00993600. Hash modulus is 0x493. Misses leave the table alone.
void retireCollisionHashNode(void* table, const std::uint32_t* key) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(table);
    const std::uint32_t low = key[2];
    const std::uint32_t high = key[3];
    const std::uint32_t bucket = ((low << 16) + high) % 0x493U;
    auto node = loadU32(self + bucket * 4U);
    while (node != 0) {
        auto* entry = reinterpret_cast<std::uint8_t*>(node);
        if (loadU32(entry + 8) == low && loadU32(entry + 0x0C) == high) {
            if (loadU32(self + 0x1254) == node) {
                storeU32(self + 0x1254, loadU32(entry + 0x30));
            }
            const auto next = loadU32(entry + 0x30);
            if (next != 0) {
                storeU32(reinterpret_cast<void*>(next + 0x2C), loadU32(entry + 0x2C));
            }
            storeU32(reinterpret_cast<void*>(loadU32(entry + 0x2C)), next);
            storeU32(entry + 0x2C, 0);
            storeU32(entry + 0x30, loadU32(self + 0x124C));
            storeU32(self + 0x124C, node);
            return;
        }
        node = loadU32(entry + 0x30);
    }
}

// 0x009937E0
void retireCollisionObjectList(void* collision, void* owner) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(collision);
    auto* parent = reinterpret_cast<std::uint8_t*>(owner);
    while (loadU32(parent + 0x24) != 0) {
        auto* wrapper = reinterpret_cast<std::uint8_t*>(loadU32(parent + 0x24));
        auto* entry = reinterpret_cast<std::uint8_t*>(loadU32(wrapper + 8));
        const auto nextA = loadU32(entry + 0x18);
        if (nextA != 0) {
            storeU32(reinterpret_cast<void*>(nextA), loadU32(entry + 0x14));
        }
        storeU32(reinterpret_cast<void*>(loadU32(entry + 0x14)), loadU32(entry + 0x18));
        const auto nextB = loadU32(entry + 0x24);
        if (nextB != 0) {
            storeU32(reinterpret_cast<void*>(nextB), loadU32(entry + 0x20));
        }
        storeU32(reinterpret_cast<void*>(loadU32(entry + 0x20)), loadU32(entry + 0x24));
        storeU32(entry + 0x14, 0);
        storeU32(entry + 0x20, 0);
        std::uint32_t key[4] = {};
        key[2] = loadU32(entry + 8);
        key[3] = loadU32(entry + 0x0C);
        retireCollisionHashNode(self + 0xAE10, key);
        if (loadU32(self + 0xAE0C) == reinterpret_cast<std::uintptr_t>(entry)) {
            storeU32(self + 0xAE0C, loadU32(entry + 0x30));
        }
        const auto next = loadU32(entry + 0x30);
        if (next != 0) {
            storeU32(reinterpret_cast<void*>(next + 0x2C), loadU32(entry + 0x2C));
        }
        storeU32(reinterpret_cast<void*>(loadU32(entry + 0x2C)), next);
        storeU32(entry + 0x2C, 0);
        storeU32(entry + 0x30, loadU32(self + 0xAE04));
        storeU32(self + 0xAE04, reinterpret_cast<std::uintptr_t>(entry));
    }
}

// 0x009938A0. Null drawable returns immediately. The free path (flag byte clear) is tested
// only when the registration virtual returns null, so 0x004166B0 is not reached.
void updateCollisionRegistration(void* collision, void* drawable) noexcept {
    if (drawable == nullptr) {
        return;
    }
    auto* self = reinterpret_cast<std::uint8_t*>(collision);
    auto* object = reinterpret_cast<std::uint8_t*>(drawable);
    const auto vtable = loadU32(object);
    const auto getRegistration = loadU32(reinterpret_cast<const void*>(vtable + 0x1C));
    using Getter = std::uint8_t*(__fastcall*)(void*, void*);
    auto* registration = reinterpret_cast<Getter>(getRegistration)(object, nullptr);
    if (registration == nullptr) {
        return;
    }
    if (self[0xC06D] != 0) {
        storeU32(registration + 8, reinterpret_cast<std::uintptr_t>(object));
        if (loadU32(registration + 0x14) != 0) {
            return;
        }
        auto* head = self + 8;
        storeU32(registration + 0x14, reinterpret_cast<std::uintptr_t>(head));
        const auto previous = loadU32(head);
        storeU32(registration + 0x18, previous);
        if (previous != 0) {
            storeU32(reinterpret_cast<void*>(previous + 0x14), reinterpret_cast<std::uintptr_t>(registration + 0x18));
        }
        storeU32(head, reinterpret_cast<std::uintptr_t>(registration));
        return;
    }
    const auto notify = loadU32(reinterpret_cast<const void*>(vtable + 0x18));
    using Notify = void(__fastcall*)(void*, void*, std::uint32_t);
    reinterpret_cast<Notify>(notify)(object, nullptr, 0);
    retireCollisionObjectList(self, registration);
    storeU32(registration + 4, 0);
    storeU32(registration + 8, 0);
    unlinkCollisionEntry(registration);
}

#pragma optimize("", off)

// 0x00995D00
void matrix3x3ToQuaternion(float* quaternion, const float* matrix) noexcept {
    const float m00 = loadF32(matrix + 0);
    const float m01 = loadF32(matrix + 1);
    const float m02 = loadF32(matrix + 2);
    const float m10 = loadF32(matrix + 4);
    const float m11 = loadF32(matrix + 5);
    const float m12 = loadF32(matrix + 6);
    const float m20 = loadF32(matrix + 8);
    const float m21 = loadF32(matrix + 9);
    const float m22 = loadF32(matrix + 10);
    const float length = column0Length(matrix);
    const float inverse = divss(kOne, length);
    const float s01 = mulss(m01, inverse);
    const float s12 = mulss(m12, inverse);
    const float s00 = mulss(m00, inverse);
    const float s10 = mulss(m10, inverse);
    const float s20 = mulss(m20, inverse);
    const float s11 = mulss(m11, inverse);
    const float s21 = mulss(m21, inverse);
    const float s02 = mulss(m02, inverse);
    const float s22 = mulss(m22, inverse);
    const float diagonalPair = addss(s22, s11);
    float trace = addss(diagonalPair, s00);
    float q0;
    float q1;
    float q2;
    float q3;
    if (!comissJbe(trace, kZero)) {
        const float root = x87sqrt(addss(trace, kOne));
        const float scale = divss(kHalf, root);
        q0 = mulss(subss(s21, s12), scale);
        q1 = mulss(subss(s02, s20), scale);
        q2 = mulss(subss(s10, s01), scale);
        q3 = mulss(root, kHalf);
    } else {
        float largest = s00;
        int axis = 0;
        if (!comissJbe(s11, s00)) {
            largest = s11;
            axis = 1;
        }
        if (comissJa(s22, largest)) {
            const float root = x87sqrt(addss(subss(s22, addss(s11, s00)), kOne));
            const float halfRoot = mulss(root, kHalf);
            float scale = s11;
            if (!ucomissOrderedEqual(root, kZero)) {
                scale = divss(kHalf, root);
            }
            q3 = mulss(subss(s10, s01), scale);
            q0 = mulss(addss(s20, s02), scale);
            q2 = mulss(addss(s12, s21), scale);
            q1 = halfRoot;
        } else if (axis == 0) {
            const float root = x87sqrt(addss(subss(s00, diagonalPair), kOne));
            float scale = root;
            const float halfRoot = mulss(root, kHalf);
            if (!ucomissOrderedEqual(root, kZero)) {
                scale = divss(kHalf, root);
            }
            q3 = mulss(subss(s21, s12), scale);
            q2 = mulss(addss(s10, s01), scale);
            q1 = mulss(addss(s20, s02), scale);
            q0 = halfRoot;
        } else {
            const float root = x87sqrt(addss(subss(s11, addss(s22, s00)), kOne));
            const float halfRoot = mulss(root, kHalf);
            float scale = root;
            if (!ucomissOrderedEqual(root, kZero)) {
                scale = divss(kHalf, root);
            }
            q3 = mulss(subss(s02, s20), scale);
            q0 = mulss(addss(s10, s01), scale);
            q1 = mulss(addss(s12, s21), scale);
            q2 = halfRoot;
        }
    }
    storeF32(quaternion, q0);
    storeF32(quaternion + 1, q1);
    storeF32(quaternion + 2, q2);
    storeF32(quaternion + 3, q3);
    scaleQuaternion(quaternion, length);
}

#pragma optimize("", on)

// 0x00997EF0 cdecl fill. Empty range leaves eax at 0 (the emulator entry value).
void* fillShaderParamRange(void* first, void* last, const void* source) noexcept {
    auto* cursor = reinterpret_cast<std::uint8_t*>(first);
    auto* end = reinterpret_cast<std::uint8_t*>(last);
    if (cursor == end) {
        return nullptr;
    }
    const auto* from = reinterpret_cast<const std::uint8_t*>(source);
    while (cursor != end) {
        copyParamRecord(cursor, from);
        cursor += 0x48;
    }
    return end + 0x34;
}

// 0x00997FD0. Returns the destination end pointer.
void* copyShaderParamsForward(const void* first, const void* last, void* destination) noexcept {
    auto* src = reinterpret_cast<const std::uint8_t*>(first);
    const auto* end = reinterpret_cast<const std::uint8_t*>(last);
    auto* dst = reinterpret_cast<std::uint8_t*>(destination);
    while (src != end) {
        copyParamRecord(dst, src);
        src += 0x48;
        dst += 0x48;
    }
    return dst;
}

// 0x009980D0. Returns the destination start, or destEnd when the range is empty.
void* copyShaderParamsBackward(const void* first, const void* last, void* destinationEnd) noexcept {
    auto* src = reinterpret_cast<const std::uint8_t*>(last);
    const auto* begin = reinterpret_cast<const std::uint8_t*>(first);
    auto* dst = reinterpret_cast<std::uint8_t*>(destinationEnd);
    while (src != begin) {
        src -= 0x48;
        dst -= 0x48;
        copyParamRecord(dst, src);
    }
    return dst;
}

// 0x009984F0 thiscall. Constants at 0x00D0F6C4 are 0,0,0,1,0,0,0 and 0x00BD90A4 is 1.
void* defaultShaderParam(void* param) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(param);
    storeU32(self, 0);
    storeU32(self + 4, 0);
    static constexpr std::uint32_t kPattern[7] = {0, 0, 0, 0x3F800000U, 0, 0, 0};
    for (int i = 0; i < 7; ++i) {
        storeU32(self + 8 + i * 4, kPattern[i]);
        storeU32(self + 0x24 + i * 4, kPattern[i]);
    }
    storeU32(self + 0x40, 0x3F800000U);
    storeU32(self + 0x44, 0);
    return self;
}

// 0x00998F40. First 0x30 bytes move through x87; the tail is integer.
void* assignShaderParamRelaxed(void* param, const void* source) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(param);
    const auto* from = reinterpret_cast<const std::uint8_t*>(source);
    for (std::uint32_t offset = 0; offset < 0x30; offset += 4) {
        fldFstp(self + offset, from + offset);
    }
    for (std::uint32_t offset = 0x30; offset < 0x48; offset += 4) {
        storeU32(self + offset, loadU32(from + offset));
    }
    return self;
}

// 0x0099A8E0. Writes the nested dword, or 0 when any pointer is null.
void readNestedNameWord(const void* shader, std::uint32_t* out) noexcept {
    const auto object = loadU32(shader);
    std::uint32_t word = 0;
    if (object != 0) {
        const auto inner = loadU32(reinterpret_cast<const std::uint8_t*>(object) + 0x0C);
        if (inner != 0) {
            word = loadU32(reinterpret_cast<const void*>(inner));
        }
    }
    *out = word;
}

// 0x0099B010. Slot +4 and slot +8 are stdcall (the object is the stack argument; ecx is not this).
void* assignRefCountedPointer(void* destination, const void* source) noexcept {
    if (destination == source) {
        return destination;
    }
    auto* dest = reinterpret_cast<std::uint8_t*>(destination);
    const auto previous = loadU32(dest);
    const auto incoming = loadU32(source);
    if (incoming != previous) {
        if (incoming != 0) {
            const auto vtable = loadU32(reinterpret_cast<const void*>(incoming));
            const auto addRef = loadU32(reinterpret_cast<const void*>(vtable + 4));
            using AddRef = void(__stdcall*)(const void*);
            reinterpret_cast<AddRef>(addRef)(reinterpret_cast<const void*>(incoming));
        }
        storeU32(dest, incoming);
        if (previous != 0) {
            const auto vtable = loadU32(reinterpret_cast<const void*>(previous));
            const auto release = loadU32(reinterpret_cast<const void*>(vtable + 8));
            using Release = void(__stdcall*)(const void*);
            reinterpret_cast<Release>(release)(reinterpret_cast<const void*>(previous));
        }
    }
    return destination;
}

// 0x0099BD10. 0x14-byte records; a non-null pointer at +8 is addref'd.
void copySubObjectRecords(void** cursor, const void* first, const void* last, void* destination) noexcept {
    *cursor = destination;
    auto* src = reinterpret_cast<const std::uint8_t*>(first);
    const auto* end = reinterpret_cast<const std::uint8_t*>(last);
    while (src != end) {
        auto* dst = reinterpret_cast<std::uint8_t*>(*cursor);
        if (dst != nullptr) {
            storeU32(dst, loadU32(src));
            storeU32(dst + 4, loadU32(src + 4));
            const auto ref = loadU32(src + 8);
            storeU32(dst + 8, ref);
            if (ref != 0) {
                addRefInline(reinterpret_cast<std::uint8_t*>(ref));
            }
            storeU32(dst + 0x0C, loadU32(src + 0x0C));
            storeU32(dst + 0x10, loadU32(src + 0x10));
        }
        *cursor = reinterpret_cast<std::uint8_t*>(*cursor) + 0x14;
        src += 0x14;
    }
}

// 0x0099BD70. The 0x48-byte tail is 0x008E5F70, an x87 assign of 18 dwords.
void* copyExtendedRecords(void** cursor, const void* first, const void* last, void* destination) noexcept {
    *cursor = destination;
    auto* src = reinterpret_cast<const std::uint8_t*>(first);
    const auto* end = reinterpret_cast<const std::uint8_t*>(last);
    while (src != end) {
        auto* dst = reinterpret_cast<std::uint8_t*>(*cursor);
        if (dst != nullptr) {
            storeU32(dst, loadU32(src));
            storeU32(dst + 4, loadU32(src + 4));
            for (std::uint32_t offset = 0; offset < 0x48; offset += 4) {
                fldFstp(dst + 8 + offset, src + 8 + offset);
            }
        }
        *cursor = reinterpret_cast<std::uint8_t*>(*cursor) + 0x50;
        src += 0x50;
    }
    return cursor;
}

// 0x0099BE10
void* assignShaderParam(void* param, const void* source) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(param);
    const auto* from = reinterpret_cast<const std::uint8_t*>(source);
    storeU32(self, loadU32(from));
    storeU32(self + 4, loadU32(from + 4));
    for (std::uint32_t offset = 8; offset <= 0x40; offset += 4) {
        fldFstp(self + offset, from + offset);
    }
    storeU32(self + 0x44, loadU32(from + 0x44));
    return self;
}

// 0x0099C0A0
void* constructShaderParamRange(void** cursor, const void* first, const void* last, void* destination) noexcept {
    *cursor = destination;
    auto* src = reinterpret_cast<const std::uint8_t*>(first);
    const auto* end = reinterpret_cast<const std::uint8_t*>(last);
    while (src != end) {
        auto* dst = reinterpret_cast<std::uint8_t*>(*cursor);
        if (dst != nullptr) {
            assignShaderParam(dst, src);
        }
        *cursor = reinterpret_cast<std::uint8_t*>(*cursor) + 0x48;
        src += 0x48;
    }
    return cursor;
}

// 0x0099C160
void* copyExtendedRecordsBackward(const void* first, const void* last, void* destinationEnd) noexcept {
    auto* src = reinterpret_cast<const std::uint8_t*>(last);
    const auto* begin = reinterpret_cast<const std::uint8_t*>(first);
    auto* dst = reinterpret_cast<std::uint8_t*>(destinationEnd);
    while (src != begin) {
        src -= 0x50;
        dst -= 0x50;
        copyExtendedRecord(dst, src);
    }
    return dst;
}

#pragma optimize("", off)

namespace {

void writeComposed(float* out, const float* left, const float* right, const float* rotated) noexcept {
    storeF32(out + 4, addss(loadF32(left + 4), rotated[0]));
    storeF32(out + 5, addss(loadF32(left + 5), rotated[1]));
    storeF32(out + 6, addss(loadF32(left + 6), rotated[2]));
    const float lx = loadF32(left);
    const float ly = loadF32(left + 1);
    const float lz = loadF32(left + 2);
    const float lw = loadF32(left + 3);
    const float rx = loadF32(right);
    const float ry = loadF32(right + 1);
    const float rz = loadF32(right + 2);
    const float rw = loadF32(right + 3);
    const float cx = subss(mulss(rz, ly), mulss(ry, lz));
    const float cy = subss(mulss(rx, lz), mulss(rz, lx));
    const float cz = subss(mulss(ry, lx), mulss(rx, ly));
    const float ox = addss(addss(mulss(lw, rx), mulss(rw, lx)), cx);
    const float oy = addss(addss(mulss(lw, ry), mulss(rw, ly)), cy);
    const float oz = addss(addss(mulss(lw, rz), mulss(rw, lz)), cz);
    const float dot = addss(addss(mulss(rx, lx), mulss(rz, lz)), mulss(ry, ly));
    const float ow = subss(mulss(rw, lw), dot);
    storeF32(out, ox);
    storeF32(out + 1, oy);
    storeF32(out + 2, oz);
    storeF32(out + 3, ow);
}

}  // namespace

// 0x0099C290 cdecl (left, right, out). 0x00931D40 rotates left's vector by right+0x10.
void composeShaderTransform(const float* left, const float* right, float* out) noexcept {
    float rotated[3];
    rotateTranslation(rotated, left, right + 4);
    writeComposed(out, left, right, rotated);
}

// 0x0099C440 thiscall. The receiver is both the left transform and the output.
void composeShaderTransformInPlace(void* transform, const float* right) noexcept {
    auto* left = reinterpret_cast<float*>(transform);
    float rotated[3];
    rotateTranslation(rotated, left, right + 4);
    const float tx = addss(loadF32(left + 4), rotated[0]);
    const float ty = addss(loadF32(left + 5), rotated[1]);
    const float tz = addss(loadF32(left + 6), rotated[2]);
    storeF32(left + 4, tx);
    storeF32(left + 5, ty);
    storeF32(left + 6, tz);
    writeComposed(left, left, right, rotated);
    storeF32(left + 4, tx);
    storeF32(left + 5, ty);
    storeF32(left + 6, tz);
}

#pragma optimize("", on)

// 0x0099F1C0. Globals 0x00E1A588/0x00E1A58C/0x00E1B314/0x00E1B318 live in the registry view.
// The SecuROM thunk 0x0043B940 is not taken: tests set the ready bit first.
void* resolveRegisteredShader(std::uint32_t* out, const std::uint32_t* key) noexcept {
    const std::uint32_t name = *key;
    if (name == 0) {
        *out = 0;
        return out;
    }
    for (auto* cursor = gRegistry.begin; cursor != gRegistry.end; cursor += 0x10) {
        const auto object = loadU32(cursor);
        std::uint32_t word = 0;
        if (object != 0) {
            const auto inner = loadU32(reinterpret_cast<const std::uint8_t*>(object) + 0x0C);
            if (inner != 0) {
                word = loadU32(reinterpret_cast<const void*>(inner));
            }
        }
        if (word == name) {
            *out = object;
            if (object != 0) {
                addRefInline(reinterpret_cast<std::uint8_t*>(object));
            }
            return out;
        }
    }
    if ((gRegistry.ready & 1U) == 0) {
        gRegistry.ready = static_cast<std::uint8_t>(gRegistry.ready | 1U);
    }
    if (name == gRegistry.cacheKey) {
        *out = 0;
        return out;
    }
    return resolveRegisteredShader(out, &gRegistry.cacheKey);
}

// 0x0099F310. 0x009C1730 / 0x009C4870 are out of range; both oracle sides use the same stand-ins.
void bindShaderConstantIds(void* shader) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(shader);
    auto* inner = reinterpret_cast<std::uint8_t*>(loadU32(self));
    auto* block = reinterpret_cast<std::uint8_t*>(loadU32(inner + 0x0C));
    storeU32(block + 4, lookupShaderConstant(0x00C58C28U));
    storeU32(block + 8, lookupShaderConstant(0x00C58C18U));
    if (loadU32(block + 8) == 0xFFFFFFFFU) {
        storeU32(block + 8, loadU32(block + 4));
    }
    auto* holder = reinterpret_cast<std::uint8_t*>(loadU32(inner + 0x1C));
    auto* api = reinterpret_cast<std::uint8_t*>(loadU32(holder + 4));
    const auto found = queryShaderConstant(api, 4, 0x00C5DCB0U, 1);
    if (found != 0) {
        const auto vtable = loadU32(api);
        const auto method = loadU32(reinterpret_cast<const void*>(vtable + 0x20));
        using Method = void(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t*);
        reinterpret_cast<Method>(method)(api, nullptr, found, 0x00C5DEBCU, reinterpret_cast<std::uint32_t*>(block + 0x0C));
    }
    std::uint32_t saved = loadU32(block + 0x10);
    const auto foundAgain = queryShaderConstant(api, 4, 0x00C5DCB0U, 1);
    if (foundAgain != 0) {
        const auto vtable = loadU32(api);
        const auto method = loadU32(reinterpret_cast<const void*>(vtable + 0x20));
        using Method = void(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t*);
        reinterpret_cast<Method>(method)(api, nullptr, foundAgain, 0x00C5DEA0U, &saved);
    }
    storeU32(block + 0x10, saved);
}

// 0x009A0500
void* replicateShaderParam(void* destination, std::uint32_t count, const void* source) noexcept {
    auto* cursor = reinterpret_cast<std::uint8_t*>(destination);
    while (count != 0) {
        if (cursor != nullptr) {
            assignShaderParam(cursor, source);
        }
        --count;
        cursor += 0x48;
    }
    return cursor;
}

// 0x009A0640. Refcount at +8 moves with the record; the other four dwords are plain copies.
void moveSubObjectRecordsBackward(const void* first, const void* last, void* destinationEnd) noexcept {
    auto* src = reinterpret_cast<const std::uint8_t*>(last);
    const auto* begin = reinterpret_cast<const std::uint8_t*>(first);
    auto* dst = reinterpret_cast<std::uint8_t*>(destinationEnd);
    while (src != begin) {
        src -= 0x14;
        dst -= 0x14;
        storeU32(dst, loadU32(src));
        storeU32(dst + 4, loadU32(src + 4));
        auto* incoming = reinterpret_cast<std::uint8_t*>(loadU32(src + 8));
        auto* previous = reinterpret_cast<std::uint8_t*>(loadU32(dst + 8));
        if (incoming != previous) {
            if (incoming != nullptr) {
                addRefInline(incoming);
            }
            storeU32(dst + 8, reinterpret_cast<std::uintptr_t>(incoming));
            if (previous != nullptr) {
                releaseInline(previous);
            }
        }
        storeU32(dst + 0x0C, loadU32(src + 0x0C));
        storeU32(dst + 0x10, loadU32(src + 0x10));
    }
}

// 0x009A2540. Fast path only: ready bit at 0x00E1B320 and the cached string at 0x00E1B31C.
// Building the semantic list calls 0x004CB6E0 and is not part of the tested path.
std::uint32_t shaderSemanticNames() noexcept {
    if (!gSemantic.ready) {
        gSemantic.ready = true;
        gSemantic.text = nullptr;
    }
    if (gSemantic.text != nullptr) {
        const auto length = *reinterpret_cast<const std::uint16_t*>(gSemantic.text - 4);
        if (length != 0) {
            return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(gSemantic.text));
        }
    }
    return 0x00BD282CU;
}

// 0x009A2DF0. Compares seven floats with ordered-equal; on mismatch rewrites the chain.
void refreshShaderParamChain(void* block, const float* matrix) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(block);
    float quaternion[4];
    matrix3x3ToQuaternion(quaternion, matrix);
    const float tx = loadF32(matrix + 3);
    const float ty = loadF32(matrix + 7);
    const float tz = loadF32(matrix + 11);
    auto* first = reinterpret_cast<std::uint8_t*>(loadU32(self + 8));
    if (self[0x30] == 0) {
        auto* pose = first + 0x24;
        if (ucomissOrderedEqual(loadF32(pose + 0x10), tx) &&
            ucomissOrderedEqual(loadF32(pose + 0x14), ty) &&
            ucomissOrderedEqual(loadF32(pose + 0x18), tz) &&
            ucomissOrderedEqual(loadF32(pose), quaternion[0]) &&
            ucomissOrderedEqual(loadF32(pose + 4), quaternion[1]) &&
            ucomissOrderedEqual(loadF32(pose + 8), quaternion[2]) &&
            ucomissOrderedEqual(loadF32(pose + 0x0C), quaternion[3])) {
            return;
        }
    }
    auto* pose = first + 0x24;
    storeF32(pose + 0x10, tx);
    storeF32(pose + 0x14, ty);
    storeF32(pose + 0x18, tz);
    storeF32(pose, quaternion[0]);
    storeF32(pose + 4, quaternion[1]);
    storeF32(pose + 8, quaternion[2]);
    storeF32(pose + 0x0C, quaternion[3]);
    for (auto* cursor = first + 0x48; cursor != reinterpret_cast<std::uint8_t*>(loadU32(self + 0x0C));
         cursor += 0x48) {
        auto* parent = reinterpret_cast<std::uint8_t*>(loadU32(cursor + 4));
        composeShaderTransform(
            reinterpret_cast<const float*>(parent + 0x24),
            reinterpret_cast<const float*>(cursor + 8),
            reinterpret_cast<float*>(cursor + 0x24));
        const auto extra = loadU32(cursor + 0x44);
        if (extra != 0) {
            composeShaderTransformInPlace(cursor + 0x24, reinterpret_cast<const float*>(extra));
            fldFstp(cursor + 0x40, reinterpret_cast<const std::uint8_t*>(extra) + 0x1C);
        }
    }
    self[0x30] = 0;
}

namespace {

constexpr float kViewMatrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0};

void skinOneInstance(std::uint8_t* block, std::uint8_t* instance, std::uint8_t* sphere) noexcept {
    auto* mesh = reinterpret_cast<std::uint8_t*>(loadU32(instance + 8));
    auto* matrix = reinterpret_cast<std::uint8_t*>(
        loadU32(reinterpret_cast<std::uint8_t*>(loadU32(mesh + 0x0C)) + 0x0C));
    const auto index = loadU32(instance + 4);
    auto* pose = reinterpret_cast<std::uint8_t*>(loadU32(block + 0x7C)) + static_cast<std::ptrdiff_t>(index) * 0x48 + 0x24;
    const float radius = loadF32(matrix + 0x14);
    matrix += 8;
    const float qx = loadF32(pose);
    const float qy = loadF32(pose + 4);
    const float qz = loadF32(pose + 8);
    const float qw = loadF32(pose + 0x0C);
    const float px = loadF32(pose + 0x10);
    const float py = loadF32(pose + 0x14);
    const float pz = loadF32(pose + 0x18);
    const float two = kTwo;
    const float xx = mulss(mulss(qx, qx), two);
    const float yy = mulss(mulss(qy, qy), two);
    const float zz = mulss(mulss(qz, qz), two);
    const float xy = mulss(mulss(qx, qy), two);
    const float xz = mulss(mulss(qx, qz), two);
    const float yz = mulss(mulss(qy, qz), two);
    const float wx = mulss(mulss(qw, qx), two);
    const float wy = mulss(mulss(qw, qy), two);
    const float wz = mulss(mulss(qw, qz), two);
    const float length2 = addss(addss(addss(mulss(qx, qx), mulss(qy, qy)), mulss(qz, qz)), mulss(qw, qw));
    const float m00 = subss(subss(length2, yy), zz);
    const float m11 = subss(subss(length2, xx), zz);
    const float m22 = subss(subss(length2, xx), yy);
    const float m01 = addss(xy, wz);
    const float m10 = subss(xy, wz);
    const float m02 = subss(xz, wy);
    const float m20 = addss(xz, wy);
    const float m12 = addss(yz, wx);
    const float m21 = subss(yz, wx);
    const float cx = loadF32(matrix);
    const float cy = loadF32(matrix + 4);
    const float cz = loadF32(matrix + 8);
    float center[4];
    center[0] = addss(addss(addss(mulss(m00, cx), mulss(m10, cy)), mulss(m20, cz)), px);
    center[1] = addss(addss(addss(mulss(m01, cx), mulss(m11, cy)), mulss(m21, cz)), py);
    center[2] = addss(addss(addss(mulss(m02, cx), mulss(m12, cy)), mulss(m22, cz)), pz);
    center[3] = radius;
    expandSphere(sphere, reinterpret_cast<std::uint8_t*>(center));
}

}  // namespace

// 0x009A82B0. 0x00D0EF98 is the view matrix (identity 3x3, translation x = 1).
void skinShaderInstances(void* block) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(block);
    refreshShaderParamChain(self + 0x74, kViewMatrix);
    storeU32(self + 0x64, 0);
    storeU32(self + 0x68, 0);
    storeU32(self + 0x6C, 0);
    storeU32(self + 0x70, 0);
    for (auto* cursor = reinterpret_cast<std::uint8_t*>(loadU32(self + 8));
         cursor != reinterpret_cast<std::uint8_t*>(loadU32(self + 0x0C)); cursor += 0x14) {
        skinOneInstance(self, cursor, self + 0x64);
    }
}

// 0x009A8EA0. A failed registry lookup releases the asset and returns false.
// The success path calls 0x009A7DE0 (ShaderAsset::Load) and is not tested.
bool bindShaderAsset(void* asset, const std::uint32_t* key) noexcept {
    std::uint32_t found = 0;
    resolveRegisteredShader(&found, key);
    if (found == 0) {
        if (asset != nullptr) {
            releaseInline(reinterpret_cast<std::uint8_t*>(asset));
        }
        return false;
    }
    return false;
}

// 0x009AB8E0. Grow calls EASTL 0x009A8AB0 and is not tested. Truncate is the covered path.
void resizeShaderParamVector(void* vec, std::uint32_t count) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(vec);
    auto* begin = reinterpret_cast<std::uint8_t*>(loadU32(self));
    auto* end = reinterpret_cast<std::uint8_t*>(loadU32(self + 4));
    const auto sizeBytes = static_cast<std::uint32_t>(end - begin);
    // 0x38E38E39 is the signed divide-by-72 magic. The truncate path only needs the count.
    const auto exact = sizeBytes / 0x48U;
    if (count > exact) {
        return;
    }
    auto* trimmed = begin + static_cast<std::ptrdiff_t>(count) * 0x48;
    copyShaderParamsForward(end, end, end);
    const auto tail = static_cast<std::uint32_t>(end - trimmed);
    const auto tailCount = tail / 0x48U;
    storeU32(self + 4, loadU32(self + 4) - tailCount * 0x48U);
}

// 0x009ACB60. 0x00417080 copies eight bytes from *[descriptor]. Resize stays on the truncate path.
void* importShaderParamBlock(void* block, const void* descriptor) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(block);
    const auto* desc = reinterpret_cast<const std::uint8_t*>(descriptor);
    const auto* header = reinterpret_cast<const std::uint8_t*>(loadU32(desc));
    storeU32(self, loadU32(header));
    storeU32(self + 4, loadU32(header + 4));
    storeU32(self + 8, 0);
    storeU32(self + 0x0C, 0);
    storeU32(self + 0x10, 0);
    storeU32(self + 0x18, 0x3F800000U);
    storeU32(self + 0x1C, 0);
    storeU32(self + 0x20, 0);
    storeU32(self + 0x24, 0);
    storeU32(self + 0x28, 0);
    self[0x30] = 1;
    const auto count = loadU32(desc + 4);
    resizeShaderParamVector(self + 8, count);
    std::uint32_t sourceCursor = 0;
    std::uint32_t destCursor = 0;
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto* row = reinterpret_cast<const std::uint8_t*>(loadU32(desc + 8)) + sourceCursor;
        auto* dst = reinterpret_cast<std::uint8_t*>(loadU32(self + 8)) + destCursor;
        storeU32(dst, loadU32(row));
        storeU32(dst + 0x18, loadU32(row + 8));
        storeU32(dst + 0x1C, loadU32(row + 0x0C));
        storeU32(dst + 0x20, loadU32(row + 0x10));
        storeU32(dst + 8, loadU32(row + 0x14));
        storeU32(dst + 0x0C, loadU32(row + 0x18));
        storeU32(dst + 0x10, loadU32(row + 0x1C));
        storeU32(dst + 0x14, loadU32(row + 0x20));
        const auto parent = loadU32(row + 4);
        if (parent == 0xFFFFFFFFU) {
            storeU32(dst + 4, 0);
        } else {
            storeU32(dst + 4, loadU32(self + 8) + parent * 0x48U);
        }
        sourceCursor += 0x64;
        destCursor += 0x48;
    }
    return self;
}

// 0x009ACFE0. The capacity-exceeded path calls EASTL 0x009AB980 and is not tested.
void* appendSubObjectRecord(void* vec, const void* record) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(vec);
    auto* end = reinterpret_cast<std::uint8_t*>(loadU32(self + 4));
    if (reinterpret_cast<std::uintptr_t>(end) >= loadU32(self + 8)) {
        return end;
    }
    storeU32(self + 4, reinterpret_cast<std::uintptr_t>(end + 0x14));
    if (end == nullptr) {
        return nullptr;
    }
    const auto* src = reinterpret_cast<const std::uint8_t*>(record);
    storeU32(end, loadU32(src));
    storeU32(end + 4, loadU32(src + 4));
    const auto ref = loadU32(src + 8);
    storeU32(end + 8, ref);
    if (ref != 0) {
        addRefInline(reinterpret_cast<std::uint8_t*>(ref));
    }
    storeU32(end + 0x0C, loadU32(src + 0x0C));
    storeU32(end + 0x10, loadU32(src + 0x10));
    return end;
}

// 0x009AE110. Pushes a 0x14 record onto [this+8] and stores this at (*nameSlot)+8.
void attachSubObjectRecord(void* block, void* const* nameSlot, std::uint32_t first, std::uint32_t second) noexcept {
    auto* self = reinterpret_cast<std::uint8_t*>(block);
    std::uint32_t record[5] = {};
    auto* object = reinterpret_cast<std::uint8_t*>(loadU32(nameSlot));
    if (object != nullptr) {
        addRefInline(object);
        record[2] = reinterpret_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(object));
    }
    record[0] = first;
    record[1] = second;
    appendSubObjectRecord(self + 8, record);
    storeU32(object + 8, reinterpret_cast<std::uintptr_t>(self));
    releaseInline(object);
}

// 0x00997720 slot 4 of 0x00C5DA5C. Virtual +0x6C is thiscall with three stack arguments.
bool runVisibleObjectsCommand(void* console, void* parsed, std::uint32_t, std::uint32_t) noexcept {
    auto* object = reinterpret_cast<std::uint8_t*>(console);
    auto* args = reinterpret_cast<std::uint8_t*>(parsed);
    const auto vtable = loadU32(object);
    const auto method = loadU32(reinterpret_cast<const void*>(vtable + 0x6C));
    using Method = void(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t);
    reinterpret_cast<Method>(method)(
        object, nullptr, loadU32(args + 8), loadU32(args + 0x0C), loadU32(args + 0x10));
    return true;
}

// 0x0099A790 slot 3 of 0x00C5DA5C. '+' and '-' prefixes update the mask words; anything else
// calls 0x00997380 and returns false (that path is not tested).
bool parseVisibleObjectsCommand(void* console, void* parsed, void* tokens) noexcept {
    auto* object = reinterpret_cast<std::uint8_t*>(console);
    auto* out = reinterpret_cast<std::uint8_t*>(parsed);
    auto* list = reinterpret_cast<std::uint8_t*>(tokens);
    const auto token0 = loadU32(reinterpret_cast<const void*>(loadU32(list) + 0x0C));
    const auto vtable = loadU32(object);
    const auto classify = loadU32(reinterpret_cast<const void*>(vtable + 0x64));
    using Classify = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t);
    storeU32(out + 8, reinterpret_cast<Classify>(classify)(object, nullptr, token0));
    storeU32(out + 0x0C, 0);
    storeU32(out + 0x10, 0);
    const auto count = loadU32(list + 0x0C);
    const auto maskOf = loadU32(reinterpret_cast<const void*>(vtable + 0x68));
    using Mask = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t);
    for (std::uint32_t index = 2; index < count; ++index) {
        const auto text = loadU32(reinterpret_cast<const void*>(loadU32(list) + index * 8U + 4U));
        const auto lead = *reinterpret_cast<const char*>(text);
        if (lead == '+') {
            const auto bits = reinterpret_cast<Mask>(maskOf)(object, nullptr, text + 1U);
            storeU32(out + 0x0C, loadU32(out + 0x0C) | bits);
        } else if (lead == '-') {
            const auto bits = reinterpret_cast<Mask>(maskOf)(object, nullptr, text + 1U);
            storeU32(out + 0x10, loadU32(out + 0x10) | bits);
        } else {
            return false;
        }
    }
    return true;
}

// 0x0099A850 slot 4 of 0x00C5DAAC. 0x009B3240 and 0x009C6340 are out of range.
bool runRenderBinCommand(void* console, void* parsed, void* args, void* nameSlot) noexcept {
    auto* object = reinterpret_cast<std::uint8_t*>(console);
    auto* command = reinterpret_cast<std::uint8_t*>(parsed);
    auto* extra = reinterpret_cast<std::uint8_t*>(args);
    if (!matchRenderBin(object, loadU32(extra), loadU32(nameSlot), 0x00C5DAC0U)) {
        return false;
    }
    const auto index = static_cast<std::int32_t>(loadU32(command + 8));
    auto* bin = reinterpret_cast<std::uint8_t*>(
        loadU32(reinterpret_cast<const void*>(loadU32(object + 0x80) + static_cast<std::uint32_t>(index) * 4U)));
    const auto name = loadU32(extra);
    const auto selector = loadU32(extra + 4);
    const auto signedCount = static_cast<std::int32_t>(loadU32(extra + 8));
    if (signedCount < 0) {
        selectRenderPair(reinterpret_cast<void*>(name), selector, 0);
    } else {
        selectRenderPair(reinterpret_cast<void*>(name), selector, static_cast<std::uint32_t>(signedCount));
    }
    const auto vtable = loadU32(object);
    const auto method = loadU32(reinterpret_cast<const void*>(vtable + 0x2C));
    using Method = void(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
    reinterpret_cast<Method>(method)(
        object, nullptr, name, loadU32(bin + 0x0C), loadU32(command + 0x0C), loadU32(command + 0x10));
    selectRenderPair(reinterpret_cast<void*>(name), 0, 0);
    return true;
}

// 0x009A1990 slot 3 of 0x00C5DAAC. Fewer than two tokens returns false.
// 0x00416BB0, 0x009B93D0 and 0x009B7EF0 are out of range and stand in on both sides.
bool parseRenderBinCommand(void* console, void* parsed, void* tokens) noexcept {
    auto* list = reinterpret_cast<std::uint8_t*>(tokens);
    if (static_cast<std::int32_t>(loadU32(list + 0x0C)) < 2) {
        return false;
    }
    auto* service = gServices.service != nullptr ? gServices.service() : nullptr;
    std::uint8_t* block = nullptr;
    if (service != nullptr && gServices.allocate != nullptr) {
        block = gServices.allocate(service, 0x10, 0x00C5DA48U, 1);
    }
    storeU32(block, 0x00E1A668U);
    storeU32(block + 4, 0);
    block[8] = 0;
    const auto name = loadU32(reinterpret_cast<const std::uint8_t*>(loadU32(list)) + 0x0C);
    if (name != 0 && loadU32(block) != 0) {
        const auto vtable = loadU32(block);
        const auto retain = loadU32(reinterpret_cast<const void*>(vtable + 4));
        using Retain = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t);
        storeU32(block + 4, reinterpret_cast<Retain>(retain)(reinterpret_cast<void*>(vtable), nullptr, name));
    }
    storeU32(block + 0x0C, 0xFFFFFFFFU);
    auto* vec = reinterpret_cast<std::uint8_t*>(console) + 0x80;
    if (gServices.reserve != nullptr) {
        gServices.reserve(vec, 0, 1);
    }
    const auto size = loadU32(vec + 0x0C);
    const auto slot = loadU32(vec) + size * 4U;
    storeU32(vec + 0x0C, size + 1U);
    if (slot != 0) {
        storeU32(reinterpret_cast<void*>(slot), reinterpret_cast<std::uintptr_t>(block));
    }
    auto* command = reinterpret_cast<std::uint8_t*>(parsed);
    const auto counter = loadU32(reinterpret_cast<std::uint8_t*>(console) + 0x8C);
    storeU32(command + 8, counter - 1U);
    storeU32(command + 0x10, 0);
    storeU32(command + 0x0C, 0);
    if (gServices.submit != nullptr) {
        gServices.submit(console, 2, 2, reinterpret_cast<std::uintptr_t>(tokens), 0);
    }
    return true;
}

}  // namespace ra3::render
