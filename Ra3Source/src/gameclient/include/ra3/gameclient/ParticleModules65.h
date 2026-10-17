#pragma once

#include <ra3/gameclient/ParticleModules.h>
#include <ra3/math/Math.h>
#include <ra3/sim/Random.h>

#include <cstddef>
#include <cstdint>

namespace ra3::gameclient {

struct Matrix33 {
    float m[9];
};

struct Matrix34 {
    float m[12];
};

static_assert(sizeof(Matrix33) == 0x24);
static_assert(sizeof(Matrix34) == 0x30);

struct AlphaKeyframeTemplateEntry {
    std::uint32_t reserved;
    sim::RandomVariable alphaRand;
    float timeFraction;
};

static_assert(sizeof(AlphaKeyframeTemplateEntry) == 0x14);

struct DefaultAlphaModuleTemplateView {
    std::uint32_t vtable;
    sim::RandomVariable keyAlphaRand[8]; // stride 0x10 in template: RandomVariable (0x0C) + float timeFraction (0x04)
};

struct AlphaTemplateKeySlot {
    sim::RandomVariable valueRand;
    float timeFraction;
};

static_assert(sizeof(AlphaTemplateKeySlot) == 0x10);

struct DefaultAlphaTemplateData {
    std::uint32_t vtable;
    AlphaTemplateKeySlot keys[8];
};

static_assert(sizeof(DefaultAlphaTemplateData) == 0x84);

struct DefaultAlphaModuleView {
    std::uint32_t vtable;
    void* system;
    const DefaultAlphaTemplateData* templatePtr;
};

struct AlphaParticleKeyframe {
    float value;
    float timeFraction;
};

static_assert(sizeof(AlphaParticleKeyframe) == 0x08);

struct FXParticleSystemTemplateStub {
    std::uint8_t pad00[0x4C];
    std::uint32_t shaderType; // +0x4C
    std::uint8_t pad50[0x30];
    sim::RandomVariable systemSizeScaleRand; // +0x80
};

static_assert(offsetof(FXParticleSystemTemplateStub, shaderType) == 0x4C);
static_assert(offsetof(FXParticleSystemTemplateStub, systemSizeScaleRand) == 0x80);

struct WindModule;

struct FXParticleSystemStub {
    std::uint32_t vtable;                        // +0x00
    std::int32_t refCount;                       // +0x04
    const FXParticleSystemTemplateStub* tmpl;    // +0x08
    std::uint8_t pad0C[0x0C];                    // +0x0C..+0x17
    WindModule* windModule;                      // +0x18
    std::uint8_t pad1C[0x80];                    // +0x1C..+0x9B
    std::uint32_t attachObjectId;                // +0x9C
    std::uint8_t padA0[0x34];                    // +0xA0..+0xD3
    Matrix34 worldTransform;                     // +0xD4..+0x103 (pos.x at +0xE0, pos.y at +0xF0, pos.z at +0x100)
    std::uint8_t pad104[0x20];                   // +0x104..+0x123
    float sizeMultiplier;                        // +0x124
    float sizeBias;                              // +0x128
    float renderObjectScaleYMultiplier;          // +0x12C
};

static_assert(offsetof(FXParticleSystemStub, tmpl) == 0x08);
static_assert(offsetof(FXParticleSystemStub, windModule) == 0x18);
static_assert(offsetof(FXParticleSystemStub, attachObjectId) == 0x9C);
static_assert(offsetof(FXParticleSystemStub, worldTransform) == 0xD4);
static_assert(offsetof(FXParticleSystemStub, sizeMultiplier) == 0x124);
static_assert(offsetof(FXParticleSystemStub, sizeBias) == 0x128);
static_assert(offsetof(FXParticleSystemStub, renderObjectScaleYMultiplier) == 0x12C);

struct FXParticleSystemRef {
    FXParticleSystemStub* system; // +0x00
    std::int32_t refCount;        // +0x04
};

static_assert(sizeof(FXParticleSystemRef) == 0x08);

struct FXParticleSpawnInfo {
    Vector3 velocity;          // +0x00
    Vector3 position;          // +0x0C
    Vector3 emitterPosition;   // +0x18
    std::uint32_t lifetime;    // +0x24
    std::uint32_t spawnFrame;  // +0x28
    std::uint8_t worldSpace;   // +0x2C
    std::uint8_t pad2D[3];
};

static_assert(sizeof(FXParticleSpawnInfo) == 0x30);

struct FXParticle {
    std::uint32_t vtable;             // +0x00
    Vector3 velocity;                 // +0x04
    Vector3 position;                 // +0x10
    Vector3 emitterPosition;          // +0x1C
    std::uint32_t initialLifetime;    // +0x28
    std::uint32_t spawnFrame;         // +0x2C
    std::uint8_t worldSpace;          // +0x30
    std::uint8_t pad31[3];
    FXParticleSystemRef* systemRef;   // +0x34
    std::uint32_t remainingLifetime;  // +0x38
    std::uint32_t activeFlag;         // +0x3C
    std::uint8_t deadFlag;            // +0x40
    std::uint8_t pad41[3];
    float randomSeedPhase;            // +0x44
    float deathTimestamp;             // +0x48
    void* colorParticleModule;        // +0x4C
    void* alphaParticleModule;        // +0x50
    void* updateParticleModule;       // +0x54
    std::uint8_t pad58[0x24];         // +0x58..+0x7B
    std::uint32_t particleId;         // +0x7C
};

static_assert(offsetof(FXParticle, velocity) == 0x04);
static_assert(offsetof(FXParticle, position) == 0x10);
static_assert(offsetof(FXParticle, initialLifetime) == 0x28);
static_assert(offsetof(FXParticle, systemRef) == 0x34);
static_assert(offsetof(FXParticle, remainingLifetime) == 0x38);
static_assert(offsetof(FXParticle, randomSeedPhase) == 0x44);
static_assert(offsetof(FXParticle, deathTimestamp) == 0x48);
static_assert(offsetof(FXParticle, updateParticleModule) == 0x54);
static_assert(offsetof(FXParticle, particleId) == 0x7C);

struct DefaultAlphaParticleModule {
    std::uint32_t vtable;           // +0x00 (0x00BF3558)
    FXParticle* particle;           // +0x04
    AlphaParticleKeyframe keys[8];  // +0x08..+0x47
    float currentAlpha;             // +0x48
    float alphaStepPerFrame;        // +0x4C
    std::int32_t nextKeyIndex;      // +0x50
};

static_assert(sizeof(DefaultAlphaParticleModule) == 0x54);
static_assert(offsetof(DefaultAlphaParticleModule, currentAlpha) == 0x48);

struct ColorKeyframe {
    float r;
    float g;
    float b;
    float timeFraction;
};

static_assert(sizeof(ColorKeyframe) == 0x10);

struct DefaultColorModuleTemplate {
    std::uint32_t vtable;             // +0x00
    ColorKeyframe keys[8];            // +0x04..+0x83
    sim::RandomVariable scaleRand;    // +0x84..+0x8F
    std::uint8_t tintEnabled;         // +0x90
    std::uint8_t pad91[3];
    float tintStrength;               // +0x94
};

static_assert(sizeof(DefaultColorModuleTemplate) == 0x98);
static_assert(offsetof(DefaultColorModuleTemplate, scaleRand) == 0x84);
static_assert(offsetof(DefaultColorModuleTemplate, tintEnabled) == 0x90);
static_assert(offsetof(DefaultColorModuleTemplate, tintStrength) == 0x94);

struct DefaultColorModule {
    std::uint32_t vtable;                         // +0x00 (0x00BF3D6C)
    FXParticleSystemStub* system;                 // +0x04
    const DefaultColorModuleTemplate* templatePtr;// +0x08
    ColorKeyframe keys[8];                        // +0x0C..+0x8B
    sim::RandomVariable scaleRand;                // +0x8C..+0x97
    float tintStrength;                           // +0x98
    std::uint8_t tintEnabled;                     // +0x9C
    std::uint8_t pad9D[3];
};

static_assert(sizeof(DefaultColorModule) == 0xA0);
static_assert(offsetof(DefaultColorModule, keys) == 0x0C);
static_assert(offsetof(DefaultColorModule, scaleRand) == 0x8C);
static_assert(offsetof(DefaultColorModule, tintStrength) == 0x98);
static_assert(offsetof(DefaultColorModule, tintEnabled) == 0x9C);

struct DefaultColorParticleModule {
    std::uint32_t vtable;           // +0x00 (0x00BF3588)
    FXParticle* particle;           // +0x04
    ColorKeyframe keys[8];          // +0x08..+0x87
    Vector3 currentColor;           // +0x88..+0x93
    Vector3 colorStepPerFrame;      // +0x94..+0x9F
    std::int32_t nextKeyIndex;      // +0xA0
};

static_assert(sizeof(DefaultColorParticleModule) == 0xA4);
static_assert(offsetof(DefaultColorParticleModule, currentColor) == 0x88);
static_assert(offsetof(DefaultColorParticleModule, colorStepPerFrame) == 0x94);
static_assert(offsetof(DefaultColorParticleModule, nextKeyIndex) == 0xA0);

struct WindModuleAssetData {
    std::uint32_t windMotion;         // +0x00
    float strength;                   // +0x04
    float fullStrengthDist;           // +0x08
    float zeroStrengthDist;           // +0x0C
    float angleChangeMin;             // +0x10
    float angleChangeMax;             // +0x14
    float pingPongStartAngleMin;      // +0x18
    float pingPongStartAngleMax;      // +0x1C
    float pingPongEndAngleMin;        // +0x20
    float pingPongEndAngleMax;        // +0x24
    float turbulenceAmplitude;        // +0x28
    float turbulenceFrequency;        // +0x2C
};

static_assert(sizeof(WindModuleAssetData) == 0x30);

struct WindModuleTemplate {
    std::uint32_t vtable;                     // +0x00 (0x00BF31BC)
    std::uint32_t windMotion;                 // +0x04
    float strength;                           // +0x08
    float fullStrengthDist;                   // +0x0C
    float zeroStrengthDist;                   // +0x10
    sim::RandomVariable angleChange;          // +0x14
    sim::RandomVariable pingPongStartAngle;   // +0x20
    sim::RandomVariable pingPongEndAngle;     // +0x2C
    float turbulenceAmplitude;                // +0x38
    float turbulenceFrequency;                // +0x3C
};

static_assert(sizeof(WindModuleTemplate) == 0x40);
static_assert(offsetof(WindModuleTemplate, angleChange) == 0x14);
static_assert(offsetof(WindModuleTemplate, pingPongStartAngle) == 0x20);
static_assert(offsetof(WindModuleTemplate, pingPongEndAngle) == 0x2C);

struct WindModule {
    std::uint32_t vtable;                 // +0x00 (0x00BF478C)
    FXParticleSystemStub* system;         // +0x04
    const WindModuleTemplate* templatePtr;// +0x08
    float startAngle;                     // +0x0C
    float currentAngle;                   // +0x10
    float angleChange;                    // +0x14
    float endAngle;                       // +0x18
    std::uint8_t increasingDirection;     // +0x1C
    std::uint8_t pad1D[3];
};

static_assert(sizeof(WindModule) == 0x20);

struct WindParticleModule {
    std::uint32_t vtable;     // +0x00 (0x00BF31D4)
    FXParticle* particle;     // +0x04
    float windRandomness;     // +0x08
};

static_assert(sizeof(WindParticleModule) == 0x0C);

struct UpdateModuleTemplate {
    std::uint32_t vtable;                 // +0x00 (0x00BF2A1C)
    sim::RandomVariable sizeRate;         // +0x04
    sim::RandomVariable sizeRateDamping;  // +0x10
    sim::RandomVariable angleZ;           // +0x1C
    sim::RandomVariable angularRateZ;     // +0x28
    sim::RandomVariable angularDamping;   // +0x34
    std::uint32_t rotationType;           // +0x40
};

static_assert(sizeof(UpdateModuleTemplate) == 0x44);

struct DefaultUpdateModuleAssetData {
    std::uint32_t reserved00;             // +0x00
    std::uint32_t rotationType;           // +0x04
    sim::RandomVariable sizeRate;         // +0x08
    sim::RandomVariable sizeRateDamping;  // +0x14
    sim::RandomVariable angleZ;           // +0x20
    sim::RandomVariable angularRateZ;     // +0x2C
    sim::RandomVariable angularDamping;   // +0x38
    sim::RandomVariable startSizeX;       // +0x44
    sim::RandomVariable startSizeY;       // +0x50
    sim::RandomVariable startSizeZ;       // +0x5C
};

static_assert(sizeof(DefaultUpdateModuleAssetData) == 0x68);

struct DefaultUpdateModuleTemplate : UpdateModuleTemplate {
    sim::RandomVariable startSizeX;       // +0x44
    sim::RandomVariable startSizeY;       // +0x50
    sim::RandomVariable startSizeZ;       // +0x5C
};

static_assert(sizeof(DefaultUpdateModuleTemplate) == 0x68);

struct DefaultUpdateModuleView {
    std::uint32_t vtable;
    FXParticleSystemRef* systemRef;
    const DefaultUpdateModuleTemplate* templatePtr;
};

struct DefaultUpdateParticleModule {
    std::uint32_t vtable;         // +0x00 (0x00BF3F9C)
    FXParticle* particle;         // +0x04
    float angleZ;                 // +0x08
    std::uint32_t rotationType;   // +0x0C
    float size;                   // +0x10
    float sizeRate;               // +0x14
    float sizeRateDamping;        // +0x18
    float angularRateZ;           // +0x1C
    float angularDamping;         // +0x20
    float xyRotationAngle;        // +0x24
    float xyAngularRate;          // +0x28
    float xyAngularDamping;       // +0x2C
};

static_assert(sizeof(DefaultUpdateParticleModule) == 0x30);

struct RenderObjectUpdateModuleAssetData {
    std::uint32_t reserved00;                 // +0x00
    std::uint32_t rotationType;               // +0x04
    sim::RandomVariable startSizeX;           // +0x08
    sim::RandomVariable startSizeZ;           // +0x14
    sim::RandomVariable startSizeY;           // +0x20
    sim::RandomVariable sizeDampingZ;         // +0x2C
    sim::RandomVariable angleZ;               // +0x38
    sim::RandomVariable angularRateZ;         // +0x44
    sim::RandomVariable angularDamping;       // +0x50
    sim::RandomVariable initialScaleX;        // +0x5C
    sim::RandomVariable initialScaleY;        // +0x68
    sim::RandomVariable initialScaleZ;        // +0x74
    sim::RandomVariable sizeRateX;            // +0x80
    sim::RandomVariable sizeRateY;            // +0x8C
    sim::RandomVariable sizeRateZ;            // +0x98
    sim::RandomVariable sizeDampingX;         // +0xA4
    sim::RandomVariable sizeDampingY;         // +0xB0
    sim::RandomVariable angleX;               // +0xBC
};

static_assert(sizeof(RenderObjectUpdateModuleAssetData) == 0xC8);

struct RenderObjectUpdateModuleTemplate : UpdateModuleTemplate {
    sim::RandomVariable initialScaleX;        // +0x44
    sim::RandomVariable initialScaleY;        // +0x50
    sim::RandomVariable initialScaleZ;        // +0x5C
    sim::RandomVariable sizeRateY;            // +0x68
    sim::RandomVariable sizeRateZ;            // +0x74
    sim::RandomVariable sizeDampingY;         // +0x80
    sim::RandomVariable sizeDampingZ;         // +0x8C
    sim::RandomVariable angleX;               // +0x98
    sim::RandomVariable angleY;               // +0xA4
    sim::RandomVariable angularRateX;         // +0xB0
    sim::RandomVariable angularRateY;         // +0xBC
};

static_assert(sizeof(RenderObjectUpdateModuleTemplate) == 0xC8);

struct RenderObjectUpdateModuleView {
    std::uint32_t vtable;
    FXParticleSystemRef* systemRef;
    const RenderObjectUpdateModuleTemplate* templatePtr;
};

struct RenderObjectParticleUpdateModule {
    std::uint32_t vtable;         // +0x00 (0x00BF4418)
    FXParticle* particle;         // +0x04
    float angleZ;                 // +0x08
    std::uint32_t rotationType;   // +0x0C
    Vector3 sizeRate;             // +0x10..+0x1B
    Vector3 sizeDamping;          // +0x1C..+0x27
    Vector3 angularRate;          // +0x28..+0x33
    float angularDamping;         // +0x34
    Vector3 scale;                // +0x38..+0x43
    float angleX;                 // +0x44
    float angleY;                 // +0x48
};

static_assert(sizeof(RenderObjectParticleUpdateModule) == 0x4C);
static_assert(offsetof(RenderObjectParticleUpdateModule, scale) == 0x38);

struct SwarmPhysicsParticleModule {
    std::uint32_t vtable;     // +0x00 (0x00BF3154)
    FXParticle* particle;     // +0x04
    float attractionStrength; // +0x08
};

static_assert(sizeof(SwarmPhysicsParticleModule) == 0x0C);

struct WavefrontVelocityAssetData {
    std::uint32_t reserved00;         // +0x00
    std::uint32_t flag04;             // +0x04
    sim::RandomVariable var0;         // +0x08
    sim::RandomVariable var1;         // +0x14
    sim::RandomVariable var2;         // +0x20
    sim::RandomVariable var3;         // +0x2C
    sim::RandomVariable var4;         // +0x38
};

static_assert(sizeof(WavefrontVelocityAssetData) == 0x44);

struct WavefrontEmissionVelocityModuleTemplate {
    std::uint32_t vtable;             // +0x00 (0x00BF2E6C)
    std::uint32_t velocityType;       // +0x04 (5)
    std::uint32_t flag08;             // +0x08
    sim::RandomVariable var0;         // +0x0C
    sim::RandomVariable var1;         // +0x18
    sim::RandomVariable var2;         // +0x24
    sim::RandomVariable var3;         // +0x30
    sim::RandomVariable var4;         // +0x3C
};

static_assert(sizeof(WavefrontEmissionVelocityModuleTemplate) == 0x48);

struct WavefrontEmissionVelocityModule {
    std::uint32_t vtable;
    void* system;
    const WavefrontEmissionVelocityModuleTemplate* templatePtr;
};

struct TerrainFireAssetData {
    std::uint32_t reserved00;         // +0x00
    std::uint8_t isHollow;            // +0x04
    std::uint8_t pad05[3];
    float extraParam;                 // +0x08
    sim::RandomVariable xVar;         // +0x0C
    sim::RandomVariable yVar;         // +0x18
    sim::RandomVariable zVar;         // +0x24
};

static_assert(sizeof(TerrainFireAssetData) == 0x30);

struct TerrainFireEmissionModuleTemplate {
    std::uint32_t vtable;             // +0x00 (0x00BF2E2C)
    std::uint32_t volumeType;         // +0x04 (6)
    std::uint8_t isHollow;            // +0x08
    std::uint8_t pad09[3];
    sim::RandomVariable xVar;         // +0x0C
    sim::RandomVariable yVar;         // +0x18
    sim::RandomVariable zVar;         // +0x24
    float extraParam;                 // +0x30
};

static_assert(sizeof(TerrainFireEmissionModuleTemplate) == 0x34);

struct TerrainFireEmissionModule {
    std::uint32_t vtable;                                 // +0x00 (0x00BF4BC0)
    void* system;                                         // +0x04
    const TerrainFireEmissionModuleTemplate* templatePtr; // +0x08
};

struct OutwardEmissionVelocityModuleTemplate {
    std::uint32_t vtable;
    std::uint32_t velocityType;
    sim::RandomVariable radialSpeed;  // +0x08
    sim::RandomVariable normalSpeed;  // +0x14
};

struct OutwardEmissionVelocityModule {
    std::uint32_t vtable;                                     // +0x00 (0x00BF4AB0)
    void* system;                                             // +0x04
    const OutwardEmissionVelocityModuleTemplate* templatePtr; // +0x08
};

struct SplineEmissionVolumeModule {
    std::uint32_t vtable;             // +0x00 (0x00BF4CC4)
    void* system;                     // +0x04
    const void* templatePtr;          // +0x08
    std::uint8_t pad0C[0x14];         // +0x0C..+0x1F
    Vector3 tangent;                  // +0x20..+0x2B
};

static_assert(offsetof(SplineEmissionVolumeModule, tangent) == 0x20);

struct LightningDrawAssetPair {
    sim::RandomVariable amplitude1;   // +0x00
    sim::RandomVariable amplitude2;   // +0x0C
    sim::RandomVariable frequency1;   // +0x18
    sim::RandomVariable frequency2;   // +0x24
    sim::RandomVariable phase1;       // +0x30
    sim::RandomVariable phase2;       // +0x3C
};

static_assert(sizeof(LightningDrawAssetPair) == 0x48);

struct LightningDrawAssetData {
    std::uint32_t reserved00;                 // +0x00
    float param04;                            // +0x04
    float param08;                            // +0x08
    float param0C;                            // +0x0C
    float param10;                            // +0x10
    float param14;                            // +0x14
    float param18;                            // +0x18
    std::uint32_t reserved1C;                 // +0x1C
    const LightningDrawAssetPair* pairsPtr;   // +0x20 (points to 2 elements = 0x90 bytes)
};

struct LightningDrawModuleTemplate {
    std::uint32_t vtable;                     // +0x00 (0x00BF38D0)
    float param04;                            // +0x04
    float param08;                            // +0x08
    float param0C;                            // +0x0C
    float param10;                            // +0x10
    float param14;                            // +0x14
    float param18;                            // +0x18
    sim::RandomVariable randomVars[12];       // +0x1C..+0xAB
};

static_assert(sizeof(LightningDrawModuleTemplate) == 0xAC);

struct LightningDrawModule {
    std::uint32_t vtable;                         // +0x00 (0x00BF4240)
    FXParticleSystemStub* system;                 // +0x04
    const LightningDrawModuleTemplate* templatePtr;// +0x08
    std::uint32_t shaderPassType;                 // +0x0C
    float sampledValues[12];                      // +0x10..+0x3F
};

static_assert(sizeof(LightningDrawModule) == 0x40);

struct LightningSwarmVertex {
    Vector3 position;         // +0x00..+0x0B
    float initialLifetime;    // +0x0C
    Vector3 velocity;         // +0x10..+0x1B
    float age;                // +0x1C
    float randomSeedPhase;    // +0x20
    float segmentIndex;       // +0x24
};

static_assert(sizeof(LightningSwarmVertex) == 0x28);

struct LightningVertexPackerModule {
    std::uint8_t pad00[0x24];
    std::int32_t segmentCount; // +0x24
};

static_assert(offsetof(LightningVertexPackerModule, segmentCount) == 0x24);

struct RenderObjectDrawModuleTemplate {
    std::uint32_t vtable;         // +0x00 (0x00BF30E0)
    std::uint8_t flag04;          // +0x04
    std::uint8_t pad05[3];
    float param08;                // +0x08
    std::uint8_t flag0C;          // +0x0C
    std::uint8_t pad0D[3];
    std::uint32_t entry0[3];      // +0x10..+0x1B
    float scale0;                 // +0x1C
    std::uint32_t entry1[3];      // +0x20..+0x2B
    float scale1;                 // +0x2C
    std::uint32_t entry2[3];      // +0x30..+0x3B
    float scale2;                 // +0x3C
};

static_assert(sizeof(RenderObjectDrawModuleTemplate) == 0x40);

struct LifeEventModuleTemplate {
    std::uint8_t pad00[0x14];
    sim::RandomVariable eventTimeRand; // +0x14
};

struct LifeEventModule {
    std::uint32_t vtable;                     // +0x00 (0x00BF4214)
    FXParticleSystemStub* system;             // +0x04
    const LifeEventModuleTemplate* templatePtr;// +0x08
    std::int32_t triggerFrame;                // +0x0C
    std::uint8_t active;                      // +0x10
    std::uint8_t pad11[3];
};

static_assert(sizeof(LifeEventModule) == 0x14);

// Alpha keyframe module family (DefaultAlphaParticleModule, vtable 0x00BF3558)
void computeDefaultAlphaRate(DefaultAlphaParticleModule& module) noexcept; // 0x0065E1E0
void updateDefaultAlphaParticle(DefaultAlphaParticleModule& module) noexcept; // 0x006650E0
DefaultAlphaParticleModule* initializeDefaultAlphaParticle(
    DefaultAlphaParticleModule& module,
    FXParticle* particle,
    const DefaultAlphaModuleView& alphaMod,
    sim::RandomState& rng) noexcept; // 0x00661370

// Color keyframe module family (DefaultColorModule 0x00BF3D6C, DefaultColorParticleModule 0x00BF3588)
DefaultColorModule* initializeDefaultColorModule(
    DefaultColorModule& module,
    FXParticleSystemStub* const* systemRef,
    const DefaultColorModuleTemplate* tmpl) noexcept; // 0x00669AB0
void getDefaultColorScaleRange(
    const DefaultColorModule& module, float (&outRange)[2]) noexcept; // 0x00669CA0
void applyDefaultColorTint(
    DefaultColorModule& module, std::uint32_t packedRgb) noexcept; // 0x0065E490
void computeDefaultColorRate(DefaultColorParticleModule& module) noexcept; // 0x0065E710
void updateDefaultColorParticle(DefaultColorParticleModule& module) noexcept; // 0x0065E8B0
DefaultColorParticleModule* initializeDefaultColorParticle(
    DefaultColorParticleModule& module,
    FXParticle* particle,
    const DefaultColorModule& colorMod,
    sim::RandomState& rng) noexcept; // 0x00661590

// Wind module family (gameclient\system\FXParticleSystem\fxpswindmodule.cpp)
WindModuleTemplate* initializeWindModuleTemplateFromAsset(
    WindModuleTemplate& tmpl, const WindModuleAssetData& asset) noexcept; // 0x0065FC70
WindModuleTemplate* copyWindModuleTemplate(
    WindModuleTemplate& dst, const WindModuleTemplate& src) noexcept; // 0x0065FCF0
WindModule* initializeWindModule(
    WindModule& module,
    FXParticleSystemStub* const* systemRef,
    const WindModuleTemplate* tmpl,
    sim::RandomState& rng) noexcept; // 0x0066CD60
void updateWindModuleMotion(
    WindModule& module, sim::RandomState& rng) noexcept; // 0x0065FD60
float getParticleSize(const FXParticle& particle) noexcept; // 0x00660530
void getParticleSizeExtended(const FXParticle& particle, std::uint8_t (&outSt0)[10]) noexcept; // 0x00660530 (10-byte ST0)
void updateWindParticle(WindParticleModule& module) noexcept; // 0x0066CE40

// Update module family (UpdateModuleTemplate, DefaultUpdate, RenderObjectUpdate)
UpdateModuleTemplate* copyUpdateModuleTemplateBase(
    UpdateModuleTemplate& dst, const UpdateModuleTemplate& src) noexcept; // 0x0065EC20
DefaultUpdateModuleTemplate* initializeDefaultUpdateModuleTemplateFromAsset(
    DefaultUpdateModuleTemplate& tmpl,
    const DefaultUpdateModuleAssetData& asset) noexcept; // 0x0065EB10
DefaultUpdateModuleTemplate* copyDefaultUpdateModuleTemplate(
    DefaultUpdateModuleTemplate& dst,
    const DefaultUpdateModuleTemplate& src) noexcept; // 0x00661B20
DefaultUpdateParticleModule* initializeDefaultUpdateParticle(
    DefaultUpdateParticleModule& module,
    FXParticle* particle,
    const DefaultUpdateModuleView& updateMod,
    float globalSizeMultiplier,
    sim::RandomState& rng) noexcept; // 0x0066A740
void updateDefaultUpdateParticle(DefaultUpdateParticleModule& module) noexcept; // 0x0065EDF0
RenderObjectUpdateModuleTemplate* initializeRenderObjectUpdateModuleTemplateFromAsset(
    RenderObjectUpdateModuleTemplate& tmpl,
    const RenderObjectUpdateModuleAssetData& asset) noexcept; // 0x0065F530
RenderObjectUpdateModuleTemplate* copyRenderObjectUpdateModuleTemplate(
    RenderObjectUpdateModuleTemplate& dst,
    const RenderObjectUpdateModuleTemplate& src) noexcept; // 0x0065F690
RenderObjectParticleUpdateModule* initializeRenderObjectUpdateParticle(
    RenderObjectParticleUpdateModule& module,
    FXParticle* particle,
    const RenderObjectUpdateModuleView& updateMod,
    sim::RandomState& rng) noexcept; // 0x0066BAE0
void updateRenderObjectUpdateParticle(
    RenderObjectParticleUpdateModule& module) noexcept; // 0x0065F900

// Swarm physics particle module (0x00BF3154)
void updateSwarmPhysicsParticle(SwarmPhysicsParticleModule& module) noexcept; // 0x00666D70

// Emission velocity & volume modules
WavefrontEmissionVelocityModuleTemplate* initializeWavefrontVelocityTemplateFromAsset(
    WavefrontEmissionVelocityModuleTemplate& tmpl,
    const WavefrontVelocityAssetData& asset) noexcept; // 0x0065F100
WavefrontEmissionVelocityModuleTemplate* copyWavefrontVelocityTemplate(
    WavefrontEmissionVelocityModuleTemplate& dst,
    const WavefrontEmissionVelocityModuleTemplate& src) noexcept; // 0x0065F160
void sampleWavefrontVelocityDimensions(
    const WavefrontEmissionVelocityModule& module,
    float& outA,
    float& outB,
    float& outC,
    float& outD,
    sim::RandomState& rng) noexcept; // 0x0065F1E0
TerrainFireEmissionModuleTemplate* initializeTerrainFireTemplateFromAsset(
    TerrainFireEmissionModuleTemplate& tmpl,
    const TerrainFireAssetData& asset) noexcept; // 0x00662A00
TerrainFireEmissionModuleTemplate* copyTerrainFireTemplate(
    TerrainFireEmissionModuleTemplate& dst,
    const TerrainFireEmissionModuleTemplate& src) noexcept; // 0x00662990
void sampleTerrainFirePoint(
    const TerrainFireEmissionModule& module,
    Vector3& outPoint,
    sim::RandomState& rng) noexcept; // 0x00662A60
PointVelocityTemplateData* copyOrthoVelocityTemplate(
    PointVelocityTemplateData& dst,
    const PointVelocityTemplateData& src) noexcept; // 0x0065EF80
Vector3* computeOutwardEmissionVelocity(
    const OutwardEmissionVelocityModule& velocityMod,
    Vector3& outVel,
    const Vector3& emissionPos,
    const VolumeModuleView& volumeMod,
    sim::RandomState& rng) noexcept; // 0x0065F040
void stepCylinderVolumeRadiusOffset(CylinderVolumeModuleView& module) noexcept; // 0x0065EF50
void computeCylinderOutwardVelocity(
    const CylinderVolumeModuleView& module,
    Vector3& outVel,
    const Vector3& pos,
    float radialSpeed,
    float normalSpeed) noexcept; // 0x00665420
void computeLineOutwardVelocity(
    const VolumeModuleView& module,
    Vector3& outVel,
    const Vector3& pos,
    float radialSpeed,
    float normalSpeed) noexcept; // 0x00665540
Vector3* computeSplineOutwardVelocity(
    const SplineEmissionVolumeModule& module,
    Vector3& outVel,
    const Vector3& pos,
    float radialSpeed,
    float normalSpeed) noexcept; // 0x006657D0
Vector3* computeRadialOutwardVelocity(
    const VolumeModuleView& module,
    Vector3& outVel,
    const Vector3& pos,
    float radialSpeed,
    float normalSpeed) noexcept; // 0x00665B60

// Lightning draw, RenderObject draw, LifeEvent & FXParticle core
LightningDrawModuleTemplate* initializeLightningDrawTemplateFromAsset(
    LightningDrawModuleTemplate& tmpl,
    const LightningDrawAssetData& asset) noexcept; // 0x00662CA0
LightningDrawModuleTemplate* copyLightningDrawTemplate(
    LightningDrawModuleTemplate& dst,
    const LightningDrawModuleTemplate& src) noexcept; // 0x00662E10
void initializeDefaultLightningDrawTemplate(
    LightningDrawModuleTemplate& tmpl) noexcept; // 0x00662F50
LightningDrawModule* initializeLightningDrawModule(
    LightningDrawModule& module,
    FXParticleSystemStub* const* systemRef,
    const LightningDrawModuleTemplate* tmpl,
    sim::RandomState& rng) noexcept; // 0x0066AF10
void packLightningParticleVertices(
    const LightningVertexPackerModule& module,
    const FXParticle& particle,
    LightningSwarmVertex* outVertices) noexcept; // 0x00662B60
RenderObjectDrawModuleTemplate* copyRenderObjectDrawTemplate(
    RenderObjectDrawModuleTemplate& dst,
    const RenderObjectDrawModuleTemplate& src) noexcept; // 0x0065F460
LifeEventModule* initializeLifeEventModule(
    LifeEventModule& module,
    FXParticleSystemStub* const* systemRef,
    const LifeEventModuleTemplate* tmpl,
    sim::RandomState& rng) noexcept; // 0x0066AE60
void initializeParticleBase(
    FXParticle& particle,
    FXParticleSystemRef* const* systemRefPtr,
    const FXParticleSpawnInfo& spawnInfo,
    std::uint32_t& activeParticleCounter) noexcept; // 0x006699D0
FXParticle* initializeParticle(
    FXParticle& particle,
    FXParticleSystemRef* const* systemRefPtr,
    const FXParticleSpawnInfo& spawnInfo,
    std::uint32_t& activeParticleCounter,
    std::uint32_t currentClientFrame,
    float frameDurationSeconds,
    sim::RandomState& rng) noexcept; // 0x0066ABF0

// Spline & Matrix34/Matrix33 helpers
void evaluateCubicHermiteSpline(
    Vector3& outPoint,
    float t,
    const Vector3& p0,
    const Vector3& p1,
    const Vector3& m0,
    const Vector3& m1) noexcept; // 0x00661030
void inverseTransformPointByMatrix34(
    const Matrix34& mat,
    const Vector3& inPoint,
    Vector3& outPoint) noexcept; // 0x00664FA0
void multiplyMatrix34(
    Matrix34& outMat,
    const Matrix34& lhs,
    const Matrix34& rhs) noexcept; // 0x00660D50
void extractTransposeRotation33(
    Matrix33& outRot,
    const Matrix34& inMat) noexcept; // 0x00660F80
void buildEulerXYZMatrix33(
    Matrix33& outRot,
    const Vector3& eulerAngles) noexcept; // 0x00669060
void transposeMatrix33(
    Matrix33& outRot,
    const Matrix33& inRot) noexcept; // 0x00669150

} // namespace ra3::gameclient
