#pragma once

#include <ra3/math/Math.h>
#include <ra3/sim/Random.h>

#include <cstddef>
#include <cstdint>

namespace ra3::gameclient {

using Vector3 = ra3::math::Vector3;

struct BoxEmissionVolumeData {
    std::uint32_t reserved[2];
    std::uint8_t hollow;
    std::uint8_t padding[3];
    Vector3 halfSize;
};

struct CylinderEmissionVolumeData {
    std::uint32_t reserved[2];
    std::uint8_t hollow;
    std::uint8_t padding[3];
    float radius;
    float radiusRate;
    float length;
    Vector3 offset;
};

struct LineEmissionVolumeData {
    std::uint32_t reserved[2];
    std::uint32_t hollow;
    Vector3 start;
    Vector3 end;
};

struct SphereEmissionVolumeData {
    std::uint32_t reserved[2];
    std::uint8_t hollow;
    std::uint8_t padding[3];
    float radius;
};

struct VolumeModuleView {
    std::uint32_t vtable;
    std::uint32_t reserved;
    const void* data;
};

struct CylinderVolumeModuleView : VolumeModuleView {
    float currentRadius;
};

struct GpuDrawDataView {
    std::uint8_t bytes[0x44];
};

struct GpuDrawModuleView {
    std::uint32_t vtable;
    std::uint32_t reserved;
    const GpuDrawDataView* data;
};

struct EmitterModuleRuntimeData {
    std::uint32_t vtable;
    std::uint32_t type;
    std::uint8_t enabled;
    std::uint8_t padding[3];
    float parameters[6];
};

struct PointVelocityTemplateData {
    std::uint32_t vtable;
    std::uint32_t type;
    sim::RandomVariable x;
    sim::RandomVariable y;
    sim::RandomVariable z;
};

struct BoxVolumeRuntimeData {
    std::uint32_t vtable;
    Vector3 halfSize;
    float rate;
    sim::RandomVariable radius;
    std::uint8_t hollow;
    std::uint8_t worldSpace;
    std::uint8_t padding[2];
};

static_assert(offsetof(VolumeModuleView, data) == 0x08);
static_assert(offsetof(CylinderVolumeModuleView, currentRadius) == 0x0C);
static_assert(offsetof(BoxEmissionVolumeData, hollow) == 0x08);
static_assert(offsetof(BoxEmissionVolumeData, halfSize) == 0x0C);
static_assert(offsetof(CylinderEmissionVolumeData, length) == 0x14);
static_assert(offsetof(CylinderEmissionVolumeData, offset) == 0x18);
static_assert(offsetof(LineEmissionVolumeData, start) == 0x0C);
static_assert(offsetof(LineEmissionVolumeData, end) == 0x18);
static_assert(offsetof(SphereEmissionVolumeData, radius) == 0x0C);
static_assert(sizeof(EmitterModuleRuntimeData) == 0x24);
static_assert(offsetof(EmitterModuleRuntimeData, parameters) == 0x0C);
static_assert(sizeof(PointVelocityTemplateData) == 0x2C);
static_assert(sizeof(BoxVolumeRuntimeData) == 0x24);
static_assert(offsetof(BoxVolumeRuntimeData, radius) == 0x14);

// Original implementations were reconstructed from ra3_1.0.game. The naming
// and geometric intent were cross-checked against GameClient/System/ParticleSys.cpp
// in the GPL Generals source; the RA3 layouts and arithmetic below come from RA3.
Vector3 randomUnitVector(sim::RandomState& state) noexcept;                         // 0x00665C40
Vector3 emitBoxPoint(const VolumeModuleView& module, sim::RandomState& state) noexcept; // 0x00661CF0
Vector3 emitCylinderPoint(
    const CylinderVolumeModuleView& module, sim::RandomState& state) noexcept;       // 0x006621A0
Vector3 emitLinePoint(const VolumeModuleView& module, sim::RandomState& state) noexcept; // 0x00662460
Vector3 emitSpherePoint(
    const VolumeModuleView& module, sim::RandomState& state) noexcept;               // 0x0066A9A0
Vector3 scaleRandomUnitVector(float radius, sim::RandomState& state) noexcept;        // 0x0066A8D0
Vector3 randomVariableUnitVector(
    const sim::RandomVariable& radius, sim::RandomState& state) noexcept;             // 0x0066A940
Vector3 cylindricalVelocity(
    const sim::RandomVariable& radial, const sim::RandomVariable& normal,
    sim::RandomState& state) noexcept;                                                // 0x00662030
Vector3 pointVelocity(
    const VolumeModuleView& module, sim::RandomState& state) noexcept;                 // 0x006625B0

void initializeCylindricalVelocityRuntime(
    EmitterModuleRuntimeData& destination,
    const std::uint8_t* source) noexcept;                                              // 0x006620A0
void initializeLineVolumeRuntime(
    EmitterModuleRuntimeData& destination,
    const std::uint8_t* source) noexcept;                                              // 0x00662330
void initializeSplineVolumeRuntime(
    EmitterModuleRuntimeData& destination,
    const std::uint8_t* source) noexcept;                                              // 0x006627C0
void initializePointVelocityTemplate(
    PointVelocityTemplateData& destination,
    const std::uint8_t* source) noexcept;                                              // 0x00662570

std::uint8_t* defaultUpdateLifetimeData(VolumeModuleView& module) noexcept;             // 0x0066A710
std::uint8_t* defaultUpdateSizeData(VolumeModuleView& module) noexcept;                 // 0x0066A720
std::uint8_t* defaultUpdateRotationData(VolumeModuleView& module) noexcept;             // 0x0066A730

void initializeDefaultAlphaTemplate(
    std::uint8_t* destination, const std::uint8_t* source) noexcept;                    // 0x00661160
bool colorWithinRange(
    const float* centerAndRange, const Vector3& value, float tolerance) noexcept;       // 0x006619F0
void initializeBoxVolumeRuntime(
    BoxVolumeRuntimeData& destination, const std::uint8_t* source) noexcept;             // 0x00661AB0
void boxVolumeDebugDimensions(
    const VolumeModuleView& module, float (&output)[2]) noexcept;                       // 0x00661B00

void shuffleDwords(
    std::uint32_t* first, std::uint32_t* last, sim::RandomState& state) noexcept;      // 0x00669190
void zeroVector3(Vector3& output) noexcept;                                            // 0x006626F0
void zeroVector4(float (&output)[4]) noexcept;                                         // 0x0066A860
float one() noexcept;                                                                  // 0x0066A850

const char* boxVolumeTypeName() noexcept;                                               // 0x00661B80
const char* cylindricalVelocityTypeName() noexcept;                                    // 0x00662020
const char* lineVolumeTypeName() noexcept;                                              // 0x00662380
const char* pointVolumeTypeName() noexcept;                                             // 0x00662670
const char* splineVolumeTypeName() noexcept;                                            // 0x00662810
const char* defaultUpdateTypeName() noexcept;                                           // 0x0066A700
const char* defaultUpdateParticleTypeName() noexcept;                                   // 0x0066A840
const char* gpuDrawTypeName() noexcept;                                                  // 0x0066AB40
const char* windTypeName() noexcept;                                                     // 0x0066CDD0
const char* defaultAlphaTemplateTypeName() noexcept;                                    // 0x006611C0
const char* windTemplateTypeName() noexcept;                                            // 0x00661200
const char* defaultAlphaParticleTypeName() noexcept;                                    // 0x00661420
const char* defaultColorParticleTypeName() noexcept;                                    // 0x006619E0

std::uint32_t gpuFramesPerRow(const GpuDrawModuleView& module) noexcept;                // 0x0066AB50
std::uint32_t gpuTotalFrames(const GpuDrawModuleView& module) noexcept;                 // 0x0066AB60
float gpuSpeedMultiplier(const GpuDrawModuleView& module) noexcept;                     // 0x0066AB70
std::uint32_t gpuSingleRow(const GpuDrawModuleView& module) noexcept;                   // 0x0066AB80
std::uint32_t gpuSortParticles(const GpuDrawModuleView& module) noexcept;               // 0x0066AB90
std::uint32_t gpuMiscWord(const GpuDrawModuleView& module) noexcept;                    // 0x0066ABA0
// The original writes only AL after loading the data pointer into EAX, so its
// upper 24 return bits deliberately retain the pointer's upper bytes.
std::uint32_t gpuEnableSoftParticles(const GpuDrawModuleView& module) noexcept;          // 0x0066ABB0
void gpuMiscValues(const GpuDrawModuleView& module, float (&output)[4]) noexcept;        // 0x0066ABC0

}  // namespace ra3::gameclient
