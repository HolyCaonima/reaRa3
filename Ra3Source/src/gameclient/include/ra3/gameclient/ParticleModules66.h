#pragma once

#include <ra3/sim/Random.h>

#include <cstddef>
#include <cstdint>

namespace ra3::gameclient {

// Debug drawer root. The drawer pointer sits at +0x70, matching the client global.
struct Fx66DebugRoot {
    std::uint8_t pad[0x70];
    void* drawer;
};

static_assert(offsetof(Fx66DebugRoot, drawer) == 0x70);

using Fx66AllocFn = void* (__cdecl*)(std::uint32_t size);
using Fx66FreeFn = void (__cdecl*)(void* block);
using Fx66Thiscall0 = void (__cdecl*)(void* self);

extern Fx66DebugRoot* g_fx66DebugRoot;
extern Fx66AllocFn g_fx66Alloc;
extern Fx66FreeFn g_fx66Free;
extern Fx66Thiscall0 g_fx66SwarmPrepare;
extern std::uint8_t g_fx66SwarmArmed;
extern void* g_fx66SwarmHeld;
extern std::uint32_t g_fx66SwarmPassGate;
extern void* g_fx66TrailGate;
extern std::uint8_t g_fx66TrailArmed;
extern void* g_fx66Scene;
extern void* g_fx66StreakSource;
extern std::int32_t g_fx66AuxSentinelRef;

constexpr std::uintptr_t kFxParticleAuxSentinel = 0x00DF88F8u;
constexpr std::uint32_t kDebugDrawColor = 0xCCAAffffu;

void debugDrawBoxEmissionVolume(const std::uint8_t* module, const float* point) noexcept;
void debugDrawCylinderEmissionVolume(const std::uint8_t* module, const float* point) noexcept;
void debugDrawLineEmissionVolume(const std::uint8_t* module, const float* point) noexcept;
void debugDrawPointEmissionVolume(const float* point) noexcept;
void debugDrawSphereEmissionVolume(const std::uint8_t* module, const float* point) noexcept;
void debugDrawSplineEmissionVolume(const std::uint8_t* module, const float* point) noexcept;
void debugDrawLightningEmissionVolume(const float* point) noexcept;

void integrateSwarmStorageRecords(std::uint8_t* module) noexcept;
std::int32_t swarmStorageRecordCount(const std::uint8_t* module) noexcept;

void swarmDrawAddRefCachedName(const std::uint8_t* module, void** out) noexcept;
void gpuDrawAddRefCachedName(const std::uint8_t* module, void** out) noexcept;
void streakTemplateAddRefGlobalName(void** out) noexcept;
void trailTemplateAddRefGlobalName(void** out) noexcept;
void assignFxParticleAuxRef(std::uint8_t* particle, void** replacement) noexcept;
void resetFxParticleAuxRef(std::uint8_t* particle) noexcept;
void releaseSwarmDrawGlobal() noexcept;
void* exchangeGpuTemplateName(const std::uint8_t* tmpl, void** out) noexcept;

void copySplineEmissionTangent(const std::uint8_t* module, float* out) noexcept;
void copyDrawModuleShaderPair(const std::uint8_t* module, std::uint32_t* out) noexcept;
std::uint8_t* drawModulePayload(const std::uint8_t* module) noexcept;
std::uint32_t drawModuleShaderWord(const std::uint8_t* module) noexcept;

void xferFxParticleCore(std::uint8_t* particle, void* xfer) noexcept;
void xferFxParticleExtended(std::uint8_t* particle, void* xfer) noexcept;

void* createDefaultPhysicsParticleModule(
    const std::uint8_t* module, void* systemArg, sim::RandomState& rng) noexcept;
void* createTerrainCollisionModule(
    std::uint8_t* tmpl, void** systemSlot, sim::RandomState& rng) noexcept;
void* cloneBoxEmissionVolumeTemplate(const std::uint8_t* tmpl) noexcept;
void* cloneDefaultPhysicsTemplate(const std::uint8_t* tmpl) noexcept;
void* cloneLifeEventTemplate(const std::uint8_t* tmpl) noexcept;

void beginTrailDrawPass(const std::uint8_t* module) noexcept;
void prepareSwarmDrawPass(const std::uint8_t* module, const std::uint8_t* arg) noexcept;
std::uint32_t submitSwarmDraw(std::uint8_t* module) noexcept;

std::uint32_t* cacheDefaultAlphaTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept;
std::uint32_t* cacheDefaultColorTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept;
std::uint32_t* cacheDefaultDrawTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept;
std::uint32_t* cacheDefaultPhysicsTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept;
std::uint32_t* cacheDefaultUpdateTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept;
std::uint32_t* cacheGpuDrawTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept;
std::uint32_t* cacheLifeEventTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept;
std::uint32_t* cacheLightningDrawTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept;
std::uint32_t* cacheLightSourceDrawTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept;
std::uint32_t* cacheSwarmDrawTemplateName(std::uint32_t& guard, std::uint32_t& slot) noexcept;

}  // namespace ra3::gameclient
