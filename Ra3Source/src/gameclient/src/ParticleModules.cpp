#include <ra3/gameclient/ParticleModules.h>

#include <cmath>
#include <cstring>
#include <xmmintrin.h>
#include <utility>

namespace ra3::gameclient {
namespace {

constexpr float kTwoPi = 6.283185482025146484375F;

float clientRandomReal(
    sim::RandomState& state, float low, float high) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    return sim::randomRealExtended(&state, low, high);
#else
    return sim::randomReal(state, low, high);
#endif
}

float clientVariableValue(
    const sim::RandomVariable& variable, sim::RandomState& state) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    return sim::clientRandomVariableExtended(&variable, &state);
#else
    return sim::clientRandomVariableValue(variable, state);
#endif
}

float cosine(float value) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    float result = 0.0F;
    __asm {
        fld value
        fcos
        fstp result
    }
    return result;
#else
    return std::cos(value);
#endif
}

float sine(float value) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    float result = 0.0F;
    __asm {
        fld value
        fsin
        fstp result
    }
    return result;
#else
    return std::sin(value);
#endif
}

float reciprocalSquareRoot(float value) noexcept
{
    if (value == 0.0F) {
        return 0.0F;
    }
#if defined(_MSC_VER) && defined(_M_IX86)
    constexpr float oneValue = 1.0F;
    float result = 0.0F;
    __asm {
        fld value
        fsqrt
        fld oneValue
        fdivrp st(1), st(0)
        fstp result
    }
    return result;
#else
    return 1.0F / std::sqrt(value);
#endif
}

template <typename T>
T readAt(const GpuDrawModuleView& module, std::size_t offset) noexcept
{
    T result{};
    std::memcpy(&result, module.data->bytes + offset, sizeof(result));
    return result;
}

}  // namespace

// ra3_1.0.game 0x00665C40.
Vector3 randomUnitVector(sim::RandomState& state) noexcept
{
    Vector3 result{};
    do {
        result.x = clientRandomReal(state, -1.0F, 1.0F);
        result.y = clientRandomReal(state, -1.0F, 1.0F);
        result.z = clientRandomReal(state, -1.0F, 1.0F);
    } while (result.x == 0.0F && result.y == 0.0F && result.z == 0.0F);

    __m128 lengthSquared = _mm_mul_ss(_mm_set_ss(result.x), _mm_set_ss(result.x));
    lengthSquared = _mm_add_ss(
        lengthSquared, _mm_mul_ss(_mm_set_ss(result.y), _mm_set_ss(result.y)));
    lengthSquared = _mm_add_ss(
        lengthSquared, _mm_mul_ss(_mm_set_ss(result.z), _mm_set_ss(result.z)));
    const float inverseLength =
        reciprocalSquareRoot(_mm_cvtss_f32(lengthSquared));
    result.x = _mm_cvtss_f32(
        _mm_mul_ss(_mm_set_ss(result.x), _mm_set_ss(inverseLength)));
    result.y = _mm_cvtss_f32(
        _mm_mul_ss(_mm_set_ss(result.y), _mm_set_ss(inverseLength)));
    result.z = _mm_cvtss_f32(
        _mm_mul_ss(_mm_set_ss(result.z), _mm_set_ss(inverseLength)));
    return result;
}

// ra3_1.0.game 0x00661CF0.
Vector3 emitBoxPoint(const VolumeModuleView& module, sim::RandomState& state) noexcept
{
    const auto& data = *static_cast<const BoxEmissionVolumeData*>(module.data);
    if (data.hollow == 0) {
        return {
            clientRandomReal(state, -data.halfSize.x, data.halfSize.x),
            clientRandomReal(state, -data.halfSize.y, data.halfSize.y),
            clientRandomReal(state, -data.halfSize.z, data.halfSize.z)};
    }

    Vector3 result{};
    const int side = sim::clientRandomInteger(state, 0, 6);
    switch (side % 3) {
    case 0:
        result.x = clientRandomReal(state, -data.halfSize.x, data.halfSize.x);
        result.y = clientRandomReal(state, -data.halfSize.y, data.halfSize.y);
        result.z = side == 0 ? -data.halfSize.z : data.halfSize.z;
        break;
    case 1:
        result.y = clientRandomReal(state, -data.halfSize.y, data.halfSize.y);
        result.z = clientRandomReal(state, -data.halfSize.z, data.halfSize.z);
        result.x = side == 1 ? -data.halfSize.x : data.halfSize.x;
        break;
    default:
        result.x = clientRandomReal(state, -data.halfSize.x, data.halfSize.x);
        result.z = clientRandomReal(state, -data.halfSize.z, data.halfSize.z);
        result.y = side == 2 ? -data.halfSize.y : data.halfSize.y;
        break;
    }
    return result;
}

// ra3_1.0.game 0x006621A0.
Vector3 emitCylinderPoint(
    const CylinderVolumeModuleView& module, sim::RandomState& state) noexcept
{
    const auto& data = *static_cast<const CylinderEmissionVolumeData*>(module.data);
    const float halfLength = data.length * 0.5F;
#if defined(_MSC_VER) && defined(_M_IX86)
    Vector3 result{};
    const float zeroValue = 0.0F;
    const float twoPiValue = kTwoPi;
    const float radiusValue = module.currentRadius;
    const float lowLength = -halfLength;
    const float offsetX = data.offset.x;
    const float offsetY = data.offset.y;
    const float offsetZ = data.offset.z;
    sim::RandomState* statePointer = &state;
    if (data.hollow != 0) {
        __asm {
            push twoPiValue
            push zeroValue
            push statePointer
            call sim::randomRealExtended
            add esp, 12
            fld radiusValue
            fxch st(1)
            fld st(0)
            fcos
            fmul st(0), st(2)
            fadd offsetX
            fstp result.x
            fsin
            fmulp st(1), st(0)
            fadd offsetY
            fstp result.y
        }
    } else {
        const float angle = clientRandomReal(state, 0.0F, kTwoPi);
        __asm {
            push radiusValue
            push zeroValue
            push statePointer
            call sim::randomRealExtended
            add esp, 12
            fld angle
            fld st(0)
            fcos
            fmul st(0), st(2)
            fadd offsetX
            fstp result.x
            fsin
            fmulp st(1), st(0)
            fadd offsetY
            fstp result.y
        }
    }
    __asm {
        push halfLength
        push lowLength
        push statePointer
        call sim::randomRealExtended
        add esp, 12
        fadd offsetZ
        fstp result.z
    }
    return result;
#else
    const float angle = clientRandomReal(state, 0.0F, kTwoPi);
    const float radius = data.hollow == 0
        ? clientRandomReal(state, 0.0F, module.currentRadius)
        : module.currentRadius;
    return {
        cosine(angle) * radius + data.offset.x,
        sine(angle) * radius + data.offset.y,
        clientRandomReal(state, -halfLength, halfLength) + data.offset.z};
#endif
}

// ra3_1.0.game 0x00662460.
Vector3 emitLinePoint(const VolumeModuleView& module, sim::RandomState& state) noexcept
{
    const auto& data = *static_cast<const LineEmissionVolumeData*>(module.data);
    const float amount = clientRandomReal(state, 0.0F, 1.0F);
    return {
        (data.end.x - data.start.x) * amount + data.start.x,
        (data.end.y - data.start.y) * amount + data.start.y,
        (data.end.z - data.start.z) * amount + data.start.z};
}

// ra3_1.0.game 0x0066A9A0.
Vector3 emitSpherePoint(const VolumeModuleView& module, sim::RandomState& state) noexcept
{
    const auto& data = *static_cast<const SphereEmissionVolumeData*>(module.data);
    const float radius =
        data.hollow == 0 ? clientRandomReal(state, 0.0F, data.radius) : data.radius;
    const Vector3 unit = randomUnitVector(state);
    return {unit.x * radius, unit.y * radius, unit.z * radius};
}

// ra3_1.0.game 0x0066A8D0.
Vector3 scaleRandomUnitVector(float radius, sim::RandomState& state) noexcept
{
    const Vector3 unit = randomUnitVector(state);
    return {unit.x * radius, unit.y * radius, unit.z * radius};
}

// ra3_1.0.game 0x0066A940.
Vector3 randomVariableUnitVector(
    const sim::RandomVariable& radius, sim::RandomState& state) noexcept
{
    const float scale = clientVariableValue(radius, state);
    const Vector3 unit = randomUnitVector(state);
#if defined(_MSC_VER) && defined(_M_IX86)
    Vector3 result{};
    __asm {
        fld unit.x
        fld scale
        fmul st(1), st(0)
        fxch st(1)
        fstp result.x
        fld unit.y
        fmul st(0), st(1)
        fstp result.y
        fmul unit.z
        fstp result.z
    }
    return result;
#else
    return {
        unit.x * scale,
        unit.y * scale,
        unit.z * scale};
#endif
}

// ra3_1.0.game 0x00662030.
Vector3 cylindricalVelocity(
    const sim::RandomVariable& radial, const sim::RandomVariable& normal,
    sim::RandomState& state) noexcept
{
    const float radialSpeed = clientVariableValue(radial, state);
    const float angle = clientRandomReal(state, 0.0F, kTwoPi);
#if defined(_MSC_VER) && defined(_M_IX86)
    Vector3 result{};
    const sim::RandomVariable* normalPointer = &normal;
    sim::RandomState* statePointer = &state;
    __asm {
        push statePointer
        push normalPointer
        call sim::clientRandomVariableExtended
        add esp, 8
        fld angle
        fld st(0)
        fcos
        fld radialSpeed
        fmul st(1), st(0)
        fxch st(1)
        fstp result.x
        fxch st(1)
        fsin
        fmulp st(1), st(0)
        fstp result.y
        fstp result.z
    }
    return result;
#else
    const float normalSpeed = clientVariableValue(normal, state);
    return {cosine(angle) * radialSpeed, sine(angle) * radialSpeed, normalSpeed};
#endif
}

// ra3_1.0.game 0x006625B0.
Vector3 pointVelocity(
    const VolumeModuleView& module, sim::RandomState& state) noexcept
{
    const auto* bytes = static_cast<const std::uint8_t*>(module.data);
    const auto& x = *reinterpret_cast<const sim::RandomVariable*>(bytes + 0x08);
    const auto& y = *reinterpret_cast<const sim::RandomVariable*>(bytes + 0x14);
    const auto& z = *reinterpret_cast<const sim::RandomVariable*>(bytes + 0x20);
    const float xValue = clientVariableValue(x, state);
    const float yValue = clientVariableValue(y, state);
    const float zValue = clientVariableValue(z, state);
    return {xValue, yValue, zValue};
}

namespace {

void initializeEmitterRuntime(
    EmitterModuleRuntimeData& destination, const std::uint8_t* source,
    std::uint32_t type, std::uint32_t vtable) noexcept
{
    destination.vtable = vtable;
    destination.type = type;
    destination.enabled = source[4];
    for (std::size_t index = 0; index < 6; ++index) {
        float value = 0.0F;
        std::memcpy(&value, source + 8 + index * sizeof(float), sizeof(value));
#if defined(_MSC_VER) && defined(_M_IX86)
        float* output = &destination.parameters[index];
        __asm {
            mov eax, output
            fld value
            fstp dword ptr [eax]
        }
#else
        destination.parameters[index] = value;
#endif
    }
}

}  // namespace

// ra3_1.0.game 0x006620A0.
void initializeCylindricalVelocityRuntime(
    EmitterModuleRuntimeData& destination, const std::uint8_t* source) noexcept
{
    initializeEmitterRuntime(destination, source, 5, 0x00BF3700);
}

// ra3_1.0.game 0x00662330.
void initializeLineVolumeRuntime(
    EmitterModuleRuntimeData& destination, const std::uint8_t* source) noexcept
{
    initializeEmitterRuntime(destination, source, 1, 0x00BF37C0);
}

// ra3_1.0.game 0x006627C0.
void initializeSplineVolumeRuntime(
    EmitterModuleRuntimeData& destination, const std::uint8_t* source) noexcept
{
    initializeEmitterRuntime(destination, source, 2, 0x00BF3864);
}

// ra3_1.0.game 0x00662570.
void initializePointVelocityTemplate(
    PointVelocityTemplateData& destination, const std::uint8_t* source) noexcept
{
    destination.vtable = 0x00BF2CFC;
    destination.type = 10;
    sim::copyRandomVariable(
        destination.x, *reinterpret_cast<const sim::RandomVariable*>(source + 4));
    sim::copyRandomVariable(
        destination.y, *reinterpret_cast<const sim::RandomVariable*>(source + 0x10));
    sim::copyRandomVariable(
        destination.z, *reinterpret_cast<const sim::RandomVariable*>(source + 0x1C));
}

// ra3_1.0.game 0x0066A710-0x0066A730.
std::uint8_t* defaultUpdateLifetimeData(VolumeModuleView& module) noexcept
{
    return static_cast<std::uint8_t*>(const_cast<void*>(module.data)) + 0x44;
}

std::uint8_t* defaultUpdateSizeData(VolumeModuleView& module) noexcept
{
    return static_cast<std::uint8_t*>(const_cast<void*>(module.data)) + 0x50;
}

std::uint8_t* defaultUpdateRotationData(VolumeModuleView& module) noexcept
{
    return static_cast<std::uint8_t*>(const_cast<void*>(module.data)) + 0x5C;
}

// ra3_1.0.game 0x00661160.
void initializeDefaultAlphaTemplate(
    std::uint8_t* destination, const std::uint8_t* source) noexcept
{
    constexpr std::uint32_t vtable = 0x00BF2AC8;
    std::memcpy(destination, &vtable, sizeof(vtable));
    std::memcpy(destination + 4, source + 4, 0x8C);
    destination[0x90] = source[0x90];
    float value = 0.0F;
    std::memcpy(&value, source + 0x94, sizeof(value));
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov eax, destination
        fld value
        fstp dword ptr [eax + 094h]
    }
#else
    std::memcpy(destination + 0x94, &value, sizeof(value));
#endif
}

// ra3_1.0.game 0x00661AB0.
void initializeBoxVolumeRuntime(
    BoxVolumeRuntimeData& destination, const std::uint8_t* source) noexcept
{
    destination.vtable = 0x00BF35B8;
    float* const fields[] = {
        &destination.halfSize.x, &destination.halfSize.y,
        &destination.halfSize.z, &destination.rate};
    const std::size_t offsets[] = {0x14, 0x18, 0x1C, 0x04};
    for (std::size_t index = 0; index < 4; ++index) {
        float value = 0.0F;
        std::memcpy(&value, source + offsets[index], sizeof(value));
#if defined(_MSC_VER) && defined(_M_IX86)
        float* output = fields[index];
        __asm {
            mov eax, output
            fld value
            fstp dword ptr [eax]
        }
#else
        *fields[index] = value;
#endif
    }
    sim::copyRandomVariable(
        destination.radius,
        *reinterpret_cast<const sim::RandomVariable*>(source + 0x08));
    destination.hollow = source[0x20];
    destination.worldSpace = source[0x21];
}

// ra3_1.0.game 0x00661B00.
void boxVolumeDebugDimensions(
    const VolumeModuleView& module, float (&output)[2]) noexcept
{
    const auto* bytes = static_cast<const std::uint8_t*>(module.data);
    float first = 0.0F;
    float second = 0.0F;
    std::memcpy(&first, bytes + 0x18, sizeof(first));
    std::memcpy(&second, bytes + 0x1C, sizeof(second));
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov eax, output
        fld first
        fstp dword ptr [eax]
        fld second
        fstp dword ptr [eax + 4]
    }
#else
    output[0] = first;
    output[1] = second;
#endif
}

// ra3_1.0.game 0x006619F0.
bool colorWithinRange(
    const float* centerAndRange, const Vector3& value, float tolerance) noexcept
{
#if defined(_MSC_VER) && defined(_M_IX86)
    bool result = false;
    const float* candidate = &value.x;
    __asm {
        mov eax, centerAndRange
        mov edx, candidate
        fld dword ptr [edx]
        fsub dword ptr [eax]
        fabs
        fld dword ptr [eax + 0Ch]
        fadd tolerance
        fcomip st(0), st(1)
        fstp st(0)
        jb outside
        fld dword ptr [edx + 4]
        fsub dword ptr [eax + 4]
        fabs
        fld dword ptr [eax + 10h]
        fadd tolerance
        fcomip st(0), st(1)
        fstp st(0)
        jb outside
        fld dword ptr [edx + 8]
        fsub dword ptr [eax + 8]
        fabs
        fld dword ptr [eax + 14h]
        fadd tolerance
        fcomip st(0), st(1)
        fstp st(0)
        jb outside
        mov result, 1
    outside:
    }
    return result;
#else
    return std::fabs(value.x - centerAndRange[0]) <= centerAndRange[3] + tolerance
        && std::fabs(value.y - centerAndRange[1]) <= centerAndRange[4] + tolerance
        && std::fabs(value.z - centerAndRange[2]) <= centerAndRange[5] + tolerance;
#endif
}

// ra3_1.0.game 0x00669190.
void shuffleDwords(
    std::uint32_t* first, std::uint32_t* last, sim::RandomState& state) noexcept
{
    if (first == last) {
        return;
    }
    for (std::uint32_t* current = first + 1; current < last; ++current) {
        const int index = sim::clientRandomInteger(
            state, 0, static_cast<int>(current - first));
        std::swap(*current, first[index]);
    }
}

void zeroVector3(Vector3& output) noexcept
{
    output = {};
}

void zeroVector4(float (&output)[4]) noexcept
{
    std::memset(output, 0, sizeof(output));
}

float one() noexcept
{
    return 1.0F;
}

const char* boxVolumeTypeName() noexcept { return "BoxEmissionVolumeModuleTemplate"; }
const char* cylindricalVelocityTypeName() noexcept
{
    return "CylindricalEmissionVelocityModuleTemplate";
}
const char* lineVolumeTypeName() noexcept { return "LineEmissionVolumeModuleTemplate"; }
const char* pointVolumeTypeName() noexcept { return "PointEmissionVolumeModuleTemplate"; }
const char* splineVolumeTypeName() noexcept { return "SplineEmissionVolumeModuleTemplate"; }
const char* defaultUpdateTypeName() noexcept { return "DefaultUpdateModule"; }
const char* defaultUpdateParticleTypeName() noexcept { return "DefaultUpdateParticleModule"; }
const char* gpuDrawTypeName() noexcept { return "GpuDrawModule"; }
const char* windTypeName() noexcept { return "WindModule"; }
const char* defaultAlphaTemplateTypeName() noexcept { return "DefaultAlphaModuleTemplate"; }
const char* windTemplateTypeName() noexcept { return "WindModuleTemplate"; }
const char* defaultAlphaParticleTypeName() noexcept { return "DefaultAlphaParticleModule"; }
const char* defaultColorParticleTypeName() noexcept { return "DefaultColorParticleModule"; }

std::uint32_t gpuFramesPerRow(const GpuDrawModuleView& module) noexcept
{
    return readAt<std::uint32_t>(module, 0x0C);
}

std::uint32_t gpuTotalFrames(const GpuDrawModuleView& module) noexcept
{
    return readAt<std::uint32_t>(module, 0x10);
}

float gpuSpeedMultiplier(const GpuDrawModuleView& module) noexcept
{
    return readAt<float>(module, 0x24);
}

std::uint32_t gpuSingleRow(const GpuDrawModuleView& module) noexcept
{
    return readAt<std::uint32_t>(module, 0x14);
}

std::uint32_t gpuSortParticles(const GpuDrawModuleView& module) noexcept
{
    return readAt<std::uint32_t>(module, 0x28);
}

std::uint32_t gpuMiscWord(const GpuDrawModuleView& module) noexcept
{
    return readAt<std::uint32_t>(module, 0x40);
}

std::uint32_t gpuEnableSoftParticles(const GpuDrawModuleView& module) noexcept
{
    const auto pointerBits = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(module.data));
    return (pointerBits & 0xFFFFFF00U) | readAt<std::uint8_t>(module, 0x2D);
}

void gpuMiscValues(const GpuDrawModuleView& module, float (&output)[4]) noexcept
{
    for (std::size_t index = 0; index < 4; ++index) {
        const float value = readAt<float>(module, 0x30 + index * sizeof(float));
#if defined(_MSC_VER) && defined(_M_IX86)
        float* destination = &output[index];
        __asm {
            mov eax, destination
            fld value
            fstp dword ptr [eax]
        }
#else
        output[index] = value;
#endif
    }
}

}  // namespace ra3::gameclient
