#include <ra3/gameclient/ParticleModules65.h>

#include <cmath>
#include <cstring>
#include <xmmintrin.h>

namespace ra3::gameclient {
namespace {

constexpr std::uint32_t kDefaultAlphaParticleVtable = 0x00BF3558u;
constexpr std::uint32_t kDefaultColorModuleBaseVtable = 0x00BF3D08u;
constexpr std::uint32_t kDefaultColorModuleVtable = 0x00BF3D6Cu;
constexpr std::uint32_t kDefaultColorParticleVtable = 0x00BF3588u;
constexpr std::uint32_t kWindModuleTemplateVtable = 0x00BF31BCu;
constexpr std::uint32_t kWindModuleBaseVtable = 0x00BF4780u;
constexpr std::uint32_t kWindModuleVtable = 0x00BF478Cu;
constexpr std::uint32_t kUpdateModuleTemplateVtable = 0x00BF2A1Cu;
constexpr std::uint32_t kDefaultUpdateModuleTemplateVtable = 0x00BF2B94u;
constexpr std::uint32_t kDefaultUpdateParticleVtable = 0x00BF3F9Cu;
constexpr std::uint32_t kRenderObjectUpdateTemplateVtable = 0x00BF3100u;
constexpr std::uint32_t kRenderObjectUpdateParticleVtable = 0x00BF4418u;
constexpr std::uint32_t kWavefrontVelocityTemplateVtable = 0x00BF2E6Cu;
constexpr std::uint32_t kTerrainFireTemplateVtable = 0x00BF2E2Cu;
constexpr std::uint32_t kOrthoVelocityTemplateVtable = 0x00BF2CFCu;
constexpr std::uint32_t kLightningDrawTemplateVtable = 0x00BF38D0u;
constexpr std::uint32_t kLightningDrawModuleBaseVtable = 0x00BF41B8u;
constexpr std::uint32_t kLightningDrawModuleVtable = 0x00BF4240u;
constexpr std::uint32_t kRenderObjectDrawTemplateVtable = 0x00BF30E0u;
constexpr std::uint32_t kLifeEventModuleBaseVtable = 0x00BF41ACu;
constexpr std::uint32_t kLifeEventModuleVtable = 0x00BF4214u;
constexpr std::uint32_t kFXParticleBaseVtable = 0x00BF3D1Cu;
constexpr std::uint32_t kFXParticleVtable = 0x00BF4170u;

constexpr float kTwoPow32 = 4294967296.0f;                 // 0x00BDF040 (0x4F800000)
constexpr float kInv255 = 0.0039215688593685627f;          // 0x00BDEF54 (0x3B808081)
constexpr float kTwoPi = 6.2831854820251465f;              // 0x00CBE96C (0x40C90FDB)
constexpr float kMinPingPongScale = 0.004999999888241291f; // 0x00BDFFA4 (0x3BA3D70A)
constexpr float kMinCylinderRadius = 0.10000000149011612f; // 0x00BD9520 (0x3DCCCCCD)
constexpr float kSwarmStepLimit = 2.0943949222564697f;     // 0x00BE089C (0x40060A91)
constexpr float kQuarter = 0.25f;                          // 0x00BD9804 (0x3E800000)
constexpr double kRadialMinDistSq = 1.0e-5;                // 0x00BE08A0

float sAdd(float a, float b) noexcept {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

float sSub(float a, float b) noexcept {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

float sMul(float a, float b) noexcept {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

float sDiv(float a, float b) noexcept {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

void x87CopyFloat(float& dst, const float& src) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    const float* s = &src;
    float* d = &dst;
    __asm {
        mov eax, s
        mov ecx, d
        fld dword ptr [eax]
        fstp dword ptr [ecx]
    }
#else
    std::memcpy(&dst, &src, sizeof(float));
#endif
}

float clientRvFloat(const sim::RandomVariable& var, sim::RandomState& rng) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    return sim::clientRandomVariableExtended(&var, &rng);
#else
    return sim::clientRandomVariableValue(var, rng);
#endif
}

float clientRandomRealFloat(sim::RandomState& rng, float low, float high) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    return sim::randomRealExtended(&rng, low, high);
#else
    return sim::randomReal(rng, low, high);
#endif
}

float x87Cos(float value) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    float result = 0.0f;
    __asm {
        fld dword ptr [value]
        fcos
        fstp dword ptr [result]
    }
    return result;
#else
    return std::cos(value);
#endif
}

float x87Sin(float value) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    float result = 0.0f;
    __asm {
        fld dword ptr [value]
        fsin
        fstp dword ptr [result]
    }
    return result;
#else
    return std::sin(value);
#endif
}

std::uint32_t x87TruncateScaledUint32(std::uint32_t count, const float& factor) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    const float* factorPtr = &factor;
    std::int32_t signedCount = static_cast<std::int32_t>(count);
    std::uint16_t savedCw = 0;
    std::uint16_t chopCw = 0;
    std::int64_t truncated = 0;
    __asm {
        fild dword ptr [signedCount]
        mov eax, signedCount
        test eax, eax
        jns skip_unsigned_adjust
        fadd dword ptr [kTwoPow32]
    skip_unsigned_adjust:
        mov ecx, factorPtr
        fmul dword ptr [ecx]
        fnstcw word ptr [savedCw]
        movzx eax, word ptr [savedCw]
        or ah, 0Ch
        mov word ptr [chopCw], ax
        fldcw word ptr [chopCw]
        fistp qword ptr [truncated]
        fldcw word ptr [savedCw]
    }
    return static_cast<std::uint32_t>(truncated);
#else
    return static_cast<std::uint32_t>(static_cast<double>(count) * static_cast<double>(factor));
#endif
}

float x87Uint32ToFloat(std::uint32_t value) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    std::int32_t signedVal = static_cast<std::int32_t>(value);
    float outVal = 0.0f;
    __asm {
        fild dword ptr [signedVal]
        mov eax, signedVal
        test eax, eax
        jns skip_unsigned_adjust
        fadd dword ptr [kTwoPow32]
    skip_unsigned_adjust:
        fstp dword ptr [outVal]
    }
    return outVal;
#else
    return static_cast<float>(value);
#endif
}

bool x87ElapsedFractionExceeds(
    std::uint32_t remainingLife, std::uint32_t initialLife, const float& threshold) noexcept {
#if defined(_MSC_VER) && defined(_M_IX86)
    std::int32_t remSigned = static_cast<std::int32_t>(remainingLife);
    std::int32_t initSigned = static_cast<std::int32_t>(initialLife);
    const float* threshPtr = &threshold;
    double remDouble = 0.0;
    std::uint16_t statusWord = 0;
    __asm {
        fild dword ptr [remSigned]
        mov eax, remSigned
        test eax, eax
        jns rem_pos
        fadd dword ptr [kTwoPow32]
    rem_pos:
        fstp qword ptr [remDouble]
        fild dword ptr [initSigned]
        mov eax, initSigned
        test eax, eax
        jns init_pos
        fadd dword ptr [kTwoPow32]
    init_pos:
        fdivr qword ptr [remDouble]
        fld1
        fsubrp st(1), st(0)
        mov ecx, threshPtr
        fld dword ptr [ecx]
        fcompp
        fnstsw ax
        mov word ptr [statusWord], ax
    }
    return (statusWord & 0x0100u) != 0u;
#else
    const double ratio = 1.0 - static_cast<double>(remainingLife) / static_cast<double>(initialLife);
    return ratio > static_cast<double>(threshold);
#endif
}

} // namespace

// 0x0065E1E0: DefaultAlphaParticleModule::computeAlphaRate
void computeDefaultAlphaRate(DefaultAlphaParticleModule& module) noexcept {
    const std::int32_t idx = module.nextKeyIndex;
    if (idx <= 0 || idx >= 8) {
        module.alphaStepPerFrame = 0.0f;
        return;
    }
    const float nextTime = module.keys[idx].timeFraction;
    if (nextTime == 0.0f) {
        module.alphaStepPerFrame = 0.0f;
        return;
    }
    const FXParticle* particle = module.particle;
    if (particle == nullptr || particle->initialLifetime == 0u || particle->remainingLifetime == 0u) {
        module.alphaStepPerFrame = 0.0f;
        return;
    }

    const std::uint32_t endFrame = x87TruncateScaledUint32(particle->initialLifetime, module.keys[idx].timeFraction);
    const std::uint32_t startFrame = x87TruncateScaledUint32(particle->initialLifetime, module.keys[idx - 1].timeFraction);
    const std::uint32_t deltaFrames = endFrame - startFrame;
    const float deltaFramesF = x87Uint32ToFloat(deltaFrames);

    x87CopyFloat(module.currentAlpha, module.keys[idx - 1].value);
    module.alphaStepPerFrame = sDiv(sSub(module.keys[idx].value, module.keys[idx - 1].value), deltaFramesF);
}

// 0x006650E0: DefaultAlphaParticleModule::updateAlpha
void updateDefaultAlphaParticle(DefaultAlphaParticleModule& module) noexcept {
    const FXParticle* particle = module.particle;
    const std::uint32_t shaderType = particle->systemRef->system->tmpl->shaderType;
    if (shaderType == 1u || shaderType == 2u) {
        return;
    }

    module.currentAlpha = sAdd(module.currentAlpha, module.alphaStepPerFrame);
    const std::int32_t idx = module.nextKeyIndex;
    if (idx < 8 &&
        particle->remainingLifetime > 0u &&
        module.keys[idx].timeFraction != 0.0f &&
        particle->initialLifetime > 0u) {
        if (x87ElapsedFractionExceeds(
                particle->remainingLifetime, particle->initialLifetime, module.keys[idx].timeFraction)) {
            module.nextKeyIndex = idx + 1;
            computeDefaultAlphaRate(module);
        }
    } else {
        module.alphaStepPerFrame = 0.0f;
    }

    if (0.0f > module.currentAlpha) {
        module.currentAlpha = 0.0f;
    } else if (module.currentAlpha > 1.0f) {
        module.currentAlpha = 1.0f;
    }
}

// 0x00661370: DefaultAlphaParticleModule::initialize
DefaultAlphaParticleModule* initializeDefaultAlphaParticle(
    DefaultAlphaParticleModule& module,
    FXParticle* particle,
    const DefaultAlphaModuleView& alphaMod,
    sim::RandomState& rng) noexcept {
    module.vtable = kDefaultAlphaParticleVtable;
    module.particle = particle;
    for (int i = 0; i < 8; ++i) {
        module.keys[i].value = 0.0f;
        module.keys[i].timeFraction = 0.0f;
    }
    for (int i = 0; i < 8; ++i) {
        const AlphaTemplateKeySlot& slot = alphaMod.templatePtr->keys[i];
        module.keys[i].value = clientRvFloat(slot.valueRand, rng);
        x87CopyFloat(module.keys[i].timeFraction, slot.timeFraction);
    }
    module.nextKeyIndex = 1;
    computeDefaultAlphaRate(module);
    return &module;
}

// 0x00669AB0: DefaultColorModule::initialize
DefaultColorModule* initializeDefaultColorModule(
    DefaultColorModule& module,
    FXParticleSystemStub* const* systemRef,
    const DefaultColorModuleTemplate* tmpl) noexcept {
    module.vtable = kDefaultColorModuleBaseVtable;
    FXParticleSystemStub* sys = *systemRef;
    module.system = sys;
    ++sys->refCount;
    module.vtable = kDefaultColorModuleVtable;
    module.templatePtr = tmpl;
    for (int i = 0; i < 8; ++i) {
        module.keys[i].r = 0.0f;
        module.keys[i].g = 0.0f;
        module.keys[i].b = 0.0f;
        module.keys[i].timeFraction = 0.0f;
    }
    module.scaleRand.distribution = 0;
    module.scaleRand.low = 0.0f;
    module.scaleRand.high = 0.0f;

    sim::copyRandomVariable(module.scaleRand, tmpl->scaleRand);
    x87CopyFloat(module.tintStrength, tmpl->tintStrength);
    module.tintEnabled = tmpl->tintEnabled;
    std::memcpy(module.keys, tmpl->keys, sizeof(module.keys));
    return &module;
}

// 0x00669CA0: DefaultColorModule::getScaleRange
void getDefaultColorScaleRange(
    const DefaultColorModule& module, float (&outRange)[2]) noexcept {
    x87CopyFloat(outRange[0], module.scaleRand.low);
    x87CopyFloat(outRange[1], module.scaleRand.high);
}

// 0x0065E490: DefaultColorModule::applyPackedColorTint
void applyDefaultColorTint(
    DefaultColorModule& module, std::uint32_t packedRgb) noexcept {
    if (module.tintEnabled == 0u) {
        return;
    }
    const float strength = module.tintStrength;
    const std::int32_t rInt = static_cast<std::int32_t>((packedRgb >> 16u) & 0xFFu);
    const std::int32_t gInt = static_cast<std::int32_t>((packedRgb >> 8u) & 0xFFu);
    const std::int32_t bInt = static_cast<std::int32_t>(packedRgb & 0xFFu);

    float rNorm = 0.0f;
    float gNorm = 0.0f;
    float bNorm = 0.0f;
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        fild dword ptr [rInt]
        fld dword ptr [kInv255]
        fmul st(1), st(0)
        fxch st(1)
        fstp dword ptr [rNorm]
        fild dword ptr [gInt]
        fmul st(0), st(1)
        fstp dword ptr [gNorm]
        fimul dword ptr [bInt]
        fstp dword ptr [bNorm]
    }
#else
    rNorm = static_cast<float>(rInt) * kInv255;
    gNorm = static_cast<float>(gInt) * kInv255;
    bNorm = static_cast<float>(bInt) * kInv255;
#endif

    const float rFactor = sAdd(sMul(sSub(rNorm, 1.0f), strength), 1.0f);
    const float gFactor = sAdd(sMul(sSub(gNorm, 1.0f), strength), 1.0f);
    const float bFactor = sAdd(sMul(sSub(bNorm, 1.0f), strength), 1.0f);

    for (int i = 0; i < 8; ++i) {
        ColorKeyframe& key = module.keys[i];
        key.r = sMul(key.r, rFactor);
        key.g = sMul(key.g, gFactor);
        key.b = sMul(key.b, bFactor);
    }
    sim::setRandomVariable(module.scaleRand, 1.0f, 1.0f, 1);
}

// 0x0065E710: DefaultColorParticleModule::computeColorRate
void computeDefaultColorRate(DefaultColorParticleModule& module) noexcept {
    const std::int32_t idx = module.nextKeyIndex;
    if (idx <= 0 || idx >= 8) {
        module.colorStepPerFrame = {0.0f, 0.0f, 0.0f};
        return;
    }
    const float nextTime = module.keys[idx].timeFraction;
    if (nextTime == 0.0f) {
        module.colorStepPerFrame = {0.0f, 0.0f, 0.0f};
        return;
    }
    const FXParticle* particle = module.particle;
    if (particle == nullptr || particle->initialLifetime == 0u || particle->remainingLifetime == 0u) {
        module.colorStepPerFrame = {0.0f, 0.0f, 0.0f};
        return;
    }

    const std::uint32_t endFrame = x87TruncateScaledUint32(particle->initialLifetime, module.keys[idx].timeFraction);
    const std::uint32_t startFrame = x87TruncateScaledUint32(particle->initialLifetime, module.keys[idx - 1].timeFraction);
    const std::uint32_t deltaFrames = endFrame - startFrame;
    float deltaFramesF = x87Uint32ToFloat(deltaFrames);
    if (1.0f > deltaFramesF) {
        deltaFramesF = 1.0f;
    }

    const ColorKeyframe& prevKey = module.keys[idx - 1];
    const ColorKeyframe& curKey = module.keys[idx];
    x87CopyFloat(module.currentColor.x, prevKey.r);
    x87CopyFloat(module.currentColor.y, prevKey.g);
    x87CopyFloat(module.currentColor.z, prevKey.b);

    const float invFrames = sDiv(1.0f, deltaFramesF);
    module.colorStepPerFrame.x = sMul(sSub(curKey.r, prevKey.r), invFrames);
    module.colorStepPerFrame.y = sMul(sSub(curKey.g, prevKey.g), invFrames);
    module.colorStepPerFrame.z = sMul(sSub(curKey.b, prevKey.b), invFrames);
}

// 0x0065E8B0: DefaultColorParticleModule::updateColor
void updateDefaultColorParticle(DefaultColorParticleModule& module) noexcept {
    module.currentColor.x = sAdd(module.currentColor.x, module.colorStepPerFrame.x);
    module.currentColor.y = sAdd(module.currentColor.y, module.colorStepPerFrame.y);
    module.currentColor.z = sAdd(module.currentColor.z, module.colorStepPerFrame.z);

    const FXParticle* particle = module.particle;
    const std::int32_t idx = module.nextKeyIndex;
    if (idx < 8 &&
        particle->remainingLifetime > 0u &&
        module.keys[idx].timeFraction != 0.0f &&
        particle->initialLifetime > 0u) {
        if (x87ElapsedFractionExceeds(
                particle->remainingLifetime, particle->initialLifetime, module.keys[idx].timeFraction)) {
            module.nextKeyIndex = idx + 1;
            computeDefaultColorRate(module);
        }
    } else {
        module.colorStepPerFrame = {0.0f, 0.0f, 0.0f};
    }

    if (0.0f > module.currentColor.x) {
        module.currentColor.x = 0.0f;
    }
    if (0.0f > module.currentColor.y) {
        module.currentColor.y = 0.0f;
    }
    if (0.0f > module.currentColor.z) {
        module.currentColor.z = 0.0f;
    }
}

// 0x00661590: DefaultColorParticleModule::initialize
DefaultColorParticleModule* initializeDefaultColorParticle(
    DefaultColorParticleModule& module,
    FXParticle* particle,
    const DefaultColorModule& colorMod,
    sim::RandomState& rng) noexcept {
    module.vtable = kDefaultColorParticleVtable;
    module.particle = particle;
    for (int i = 0; i < 8; ++i) {
        module.keys[i] = {0.0f, 0.0f, 0.0f, 0.0f};
    }
    module.currentColor = {0.0f, 0.0f, 0.0f};
    module.colorStepPerFrame = {0.0f, 0.0f, 0.0f};

    std::memcpy(module.keys, colorMod.keys, sizeof(module.keys));

    float scaleRange[2] = {0.0f, 0.0f};
    getDefaultColorScaleRange(colorMod, scaleRange);
    if (scaleRange[0] != 1.0f || scaleRange[1] != 1.0f) {
        const float scale = clientRvFloat(colorMod.scaleRand, rng);
        for (int i = 0; i < 8; ++i) {
            module.keys[i].r = sMul(module.keys[i].r, scale);
            module.keys[i].g = sMul(module.keys[i].g, scale);
            module.keys[i].b = sMul(module.keys[i].b, scale);
        }
    }

    x87CopyFloat(module.currentColor.x, module.keys[0].r);
    x87CopyFloat(module.currentColor.y, module.keys[0].g);
    x87CopyFloat(module.currentColor.z, module.keys[0].b);
    module.nextKeyIndex = 1;
    computeDefaultColorRate(module);
    return &module;
}

// 0x0065FC70: WindModuleTemplate::initializeFromAsset
WindModuleTemplate* initializeWindModuleTemplateFromAsset(
    WindModuleTemplate& tmpl, const WindModuleAssetData& asset) noexcept {
    tmpl.vtable = kWindModuleTemplateVtable;
    tmpl.windMotion = asset.windMotion;
    x87CopyFloat(tmpl.strength, asset.strength);
    x87CopyFloat(tmpl.fullStrengthDist, asset.fullStrengthDist);
    x87CopyFloat(tmpl.zeroStrengthDist, asset.zeroStrengthDist);
    sim::setRandomVariable(tmpl.angleChange, asset.angleChangeMin, asset.angleChangeMax, 1);
    sim::setRandomVariable(tmpl.pingPongStartAngle, asset.pingPongStartAngleMin, asset.pingPongStartAngleMax, 1);
    sim::setRandomVariable(tmpl.pingPongEndAngle, asset.pingPongEndAngleMin, asset.pingPongEndAngleMax, 1);
    x87CopyFloat(tmpl.turbulenceAmplitude, asset.turbulenceAmplitude);
    x87CopyFloat(tmpl.turbulenceFrequency, asset.turbulenceFrequency);
    return &tmpl;
}

// 0x0065FCF0: WindModuleTemplate::copyFrom
WindModuleTemplate* copyWindModuleTemplate(
    WindModuleTemplate& dst, const WindModuleTemplate& src) noexcept {
    dst.vtable = kWindModuleTemplateVtable;
    dst.windMotion = src.windMotion;
    x87CopyFloat(dst.strength, src.strength);
    x87CopyFloat(dst.fullStrengthDist, src.fullStrengthDist);
    x87CopyFloat(dst.zeroStrengthDist, src.zeroStrengthDist);
    sim::copyRandomVariable(dst.angleChange, src.angleChange);
    sim::copyRandomVariable(dst.pingPongStartAngle, src.pingPongStartAngle);
    sim::copyRandomVariable(dst.pingPongEndAngle, src.pingPongEndAngle);
    x87CopyFloat(dst.turbulenceAmplitude, src.turbulenceAmplitude);
    x87CopyFloat(dst.turbulenceFrequency, src.turbulenceFrequency);
    return &dst;
}

// 0x0066CD60: WindModule::initialize (gameclient\system\FXParticleSystem\fxpswindmodule.cpp)
WindModule* initializeWindModule(
    WindModule& module,
    FXParticleSystemStub* const* systemRef,
    const WindModuleTemplate* tmpl,
    sim::RandomState& rng) noexcept {
    module.vtable = kWindModuleBaseVtable;
    FXParticleSystemStub* sys = *systemRef;
    module.system = sys;
    ++sys->refCount;
    module.vtable = kWindModuleVtable;
    module.templatePtr = tmpl;
    module.angleChange = 0.0f;
    module.increasingDirection = 1u;
    module.startAngle = clientRvFloat(tmpl->pingPongStartAngle, rng);
    module.endAngle = clientRvFloat(tmpl->pingPongEndAngle, rng);
    module.currentAngle = clientRandomRealFloat(rng, module.startAngle, module.endAngle);
    return &module;
}

// 0x0065FD60: WindModule::updateWindMotion
void updateWindModuleMotion(
    WindModule& module, sim::RandomState& rng) noexcept {
    const WindModuleTemplate* tmpl = module.templatePtr;
    if (tmpl->windMotion == 2u) {
        const float startAngle = module.startAngle;
        const float endAngle = module.endAngle;
        const float halfSpan = sMul(sSub(endAngle, startAngle), 0.5f);
        float step = 0.0f;
#if defined(_MSC_VER) && defined(_M_IX86)
        const float curAngle = module.currentAngle;
        const float angChange = module.angleChange;
        float rawStep = 0.0f;
        std::uint8_t useMin = 0u;
        __asm {
            fld dword ptr [halfSpan]
            fld st(0)
            fsub dword ptr [curAngle]
            fadd dword ptr [startAngle]
            fabs
            fdivrp st(1), st(0)
            fld1
            fsubrp st(1), st(0)
            fmul dword ptr [angChange]
            fst dword ptr [rawStep]
            fld dword ptr [kMinPingPongScale]
            _emit 0xDF
            _emit 0xF1
            fstp st(0)
            seta useMin
        }
        step = (useMin != 0u) ? kMinPingPongScale : rawStep;
#else
        const float distFromMid = std::fabs((halfSpan - module.currentAngle) + startAngle);
        step = static_cast<float>((1.0 - static_cast<double>(distFromMid) / static_cast<double>(halfSpan)) * module.angleChange);
        if (kMinPingPongScale > step) {
            step = kMinPingPongScale;
        }
#endif
        bool hitBound = false;
        if (module.increasingDirection != 0u) {
            const float nextAngle = sAdd(module.currentAngle, step);
            module.currentAngle = nextAngle;
            if (nextAngle >= endAngle) {
                module.increasingDirection = 0u;
                hitBound = true;
            }
        } else {
            const float nextAngle = sSub(module.currentAngle, step);
            module.currentAngle = nextAngle;
            if (startAngle >= nextAngle) {
                module.increasingDirection = 1u;
                hitBound = true;
            }
        }
        if (hitBound) {
            module.angleChange = clientRvFloat(tmpl->angleChange, rng);
            module.startAngle = clientRvFloat(tmpl->pingPongStartAngle, rng);
            module.endAngle = clientRvFloat(tmpl->pingPongEndAngle, rng);
        }
    } else if (tmpl->windMotion == 3u) {
        if (module.angleChange == 0.0f) {
            module.angleChange = clientRvFloat(tmpl->angleChange, rng);
        }
        const float nextAngle = sAdd(module.currentAngle, module.angleChange);
        module.currentAngle = nextAngle;
        if (nextAngle > kTwoPi) {
            module.currentAngle = sSub(nextAngle, kTwoPi);
        } else if (0.0f > nextAngle) {
            module.currentAngle = sAdd(nextAngle, kTwoPi);
        }
    }
}

// 0x00660530: FXParticle::getSize
void getParticleSizeExtended(const FXParticle& particle, std::uint8_t (&outSt0)[10]) noexcept {
    std::memset(outSt0, 0, sizeof(outSt0));
    const void* updateMod = particle.updateParticleModule;
    if (updateMod == nullptr) {
        return;
    }
    const std::uint32_t vt = *static_cast<const std::uint32_t*>(updateMod);
    const float* sizePtr = nullptr;
    if (vt == kDefaultUpdateParticleVtable) {
        sizePtr = &static_cast<const DefaultUpdateParticleModule*>(updateMod)->size;
    } else if (vt == kRenderObjectUpdateParticleVtable) {
        sizePtr = &static_cast<const RenderObjectParticleUpdateModule*>(updateMod)->scale.x;
    } else {
        return;
    }
#if defined(_MSC_VER) && defined(_M_IX86)
    std::uint8_t* dst = outSt0;
    __asm {
        mov eax, sizePtr
        mov ecx, dst
        fld dword ptr [eax]
        fstp tbyte ptr [ecx]
    }
#else
    (void)sizePtr;
#endif
}

float getParticleSize(const FXParticle& particle) noexcept {
    std::uint8_t st0[10]{};
    getParticleSizeExtended(particle, st0);
#if defined(_MSC_VER) && defined(_M_IX86)
    float outVal = 0.0f;
    std::uint8_t* src = st0;
    __asm {
        mov eax, src
        fld tbyte ptr [eax]
        fstp dword ptr [outVal]
    }
    return outVal;
#else
    return 0.0f;
#endif
}

// 0x0066CE40: WindParticleModule::updateParticleWind
void updateWindParticle(WindParticleModule& module) noexcept {
    FXParticle* particle = module.particle;
    FXParticleSystemRef* sysRef = particle->systemRef;
    const FXParticleSystemStub* sys = sysRef->system;
    const WindModule* windMod = sys->windModule;
    if (windMod != nullptr && windMod->templatePtr->windMotion == 1u) {
        return;
    }

    const float currentAngle = (windMod != nullptr) ? windMod->currentAngle : 0.0f;
    const float originX = sys->worldTransform.m[3];
    const float originY = sys->worldTransform.m[7];
    const float originZ = sys->worldTransform.m[11];

    const float dx = sSub(particle->position.x, originX);
    const float dy = sSub(particle->position.y, originY);
    const float dz = sSub(particle->position.z, originZ);
    const float distSq = sAdd(sAdd(sMul(dz, dz), sMul(dy, dy)), sMul(dx, dx));

    const float zeroDist = (windMod != nullptr) ? windMod->templatePtr->zeroStrengthDist : 0.0f;
    const float zeroDistSq = sMul(zeroDist, zeroDist);
    if (zeroDistSq <= distSq) {
        return;
    }

    const float strength = (windMod != nullptr) ? windMod->templatePtr->strength : 0.0f;
    float effectiveStrength = sMul(module.windRandomness, strength);

    const float fullDist = (windMod != nullptr) ? windMod->templatePtr->zeroStrengthDist : 0.0f;
    const float fullDistSq = sMul(fullDist, fullDist);
    if (distSq > fullDistSq) {
        float dist = 0.0f;
#if defined(_MSC_VER) && defined(_M_IX86)
        __asm {
            fld dword ptr [distSq]
            fsqrt
            fstp dword ptr [dist]
        }
#else
        dist = std::sqrt(distSq);
#endif
        const float ratio = sDiv(sSub(dist, fullDist), sSub(zeroDist, fullDist));
        effectiveStrength = sMul(sSub(1.0f, ratio), effectiveStrength);
    }

#if defined(_MSC_VER) && defined(_M_IX86)
    float* posPtr = &particle->position.x;
    __asm {
        mov eax, posPtr
        fld dword ptr [currentAngle]
        fld st(0)
        fcos
        fld dword ptr [effectiveStrength]
        fmul st(1), st(0)
        fxch st(1)
        fadd dword ptr [eax]
        fstp dword ptr [eax]
        fxch st(1)
        fsin
        fmulp st(1), st(0)
        fadd dword ptr [eax + 4]
        fstp dword ptr [eax + 4]
    }
#else
    particle->position.x += std::cos(currentAngle) * effectiveStrength;
    particle->position.y += std::sin(currentAngle) * effectiveStrength;
#endif

    if (windMod != nullptr) {
        const WindModuleTemplate* tmpl = windMod->templatePtr;
        if (tmpl->turbulenceAmplitude > 0.0f) {
            std::uint8_t sizeSt0[10]{};
            getParticleSizeExtended(*particle, sizeSt0);
#if defined(_MSC_VER) && defined(_M_IX86)
            const float posZ = particle->position.z;
            const float turbFreq = tmpl->turbulenceFrequency;
            const float turbAmp = tmpl->turbulenceAmplitude;
            const std::int32_t partIdSigned = static_cast<std::int32_t>(particle->particleId);
            std::uint8_t* st0Ptr = sizeSt0;
            float phase = 0.0f;
            __asm {
                fld dword ptr [posZ]
                fmul dword ptr [turbFreq]
                fild dword ptr [partIdSigned]
                mov edx, partIdSigned
                test edx, edx
                jge part_id_nonneg
                fadd dword ptr [kTwoPow32]
            part_id_nonneg:
                faddp st(1), st(0)
                fstp dword ptr [phase]
                mov eax, st0Ptr
                fld tbyte ptr [eax]
                fmul dword ptr [turbAmp]
                mov eax, posPtr
                fld dword ptr [phase]
                fld st(0)
                fcos
                fmul st(0), st(2)
                fadd dword ptr [eax]
                fstp dword ptr [eax]
                fsin
                fmulp st(1), st(0)
                fadd dword ptr [eax + 4]
                fstp dword ptr [eax + 4]
            }
#else
            const float phase = particle->position.z * tmpl->turbulenceFrequency + static_cast<float>(particle->particleId);
            const float amp = getParticleSize(*particle) * tmpl->turbulenceAmplitude;
            particle->position.x += std::cos(phase) * amp;
            particle->position.y += std::sin(phase) * amp;
#endif
        }
    }
}

// 0x0065EC20: UpdateModuleTemplate::copyFrom
UpdateModuleTemplate* copyUpdateModuleTemplateBase(
    UpdateModuleTemplate& dst, const UpdateModuleTemplate& src) noexcept {
    dst.vtable = kUpdateModuleTemplateVtable;
    sim::copyRandomVariable(dst.sizeRate, src.sizeRate);
    sim::copyRandomVariable(dst.sizeRateDamping, src.sizeRateDamping);
    sim::copyRandomVariable(dst.angleZ, src.angleZ);
    sim::copyRandomVariable(dst.angularRateZ, src.angularRateZ);
    sim::copyRandomVariable(dst.angularDamping, src.angularDamping);
    dst.rotationType = src.rotationType;
    return &dst;
}

// 0x0065EB10: DefaultUpdateModuleTemplate::initializeFromAsset
DefaultUpdateModuleTemplate* initializeDefaultUpdateModuleTemplateFromAsset(
    DefaultUpdateModuleTemplate& tmpl,
    const DefaultUpdateModuleAssetData& asset) noexcept {
    tmpl.vtable = kUpdateModuleTemplateVtable;
    sim::setRandomVariable(tmpl.sizeRate, 0.0f, 0.0f, 0);
    sim::setRandomVariable(tmpl.sizeRateDamping, 0.0f, 0.0f, 0);
    sim::setRandomVariable(tmpl.angleZ, 0.0f, 0.0f, 0);
    sim::setRandomVariable(tmpl.angularRateZ, 0.0f, 0.0f, 0);
    sim::setRandomVariable(tmpl.angularDamping, 0.0f, 0.0f, 0);
    tmpl.rotationType = 0u;
    tmpl.vtable = kDefaultUpdateModuleTemplateVtable;
    sim::setRandomVariable(tmpl.startSizeX, 0.0f, 0.0f, 0);
    sim::setRandomVariable(tmpl.startSizeY, 0.0f, 0.0f, 0);
    sim::setRandomVariable(tmpl.startSizeZ, 0.0f, 0.0f, 0);

    tmpl.rotationType = asset.rotationType;
    sim::copyRandomVariable(tmpl.sizeRate, asset.sizeRate);
    sim::copyRandomVariable(tmpl.sizeRateDamping, asset.sizeRateDamping);
    sim::copyRandomVariable(tmpl.angleZ, asset.angleZ);
    sim::copyRandomVariable(tmpl.angularRateZ, asset.angularRateZ);
    sim::copyRandomVariable(tmpl.angularDamping, asset.angularDamping);
    sim::copyRandomVariable(tmpl.startSizeX, asset.startSizeX);
    sim::copyRandomVariable(tmpl.startSizeY, asset.startSizeY);
    sim::copyRandomVariable(tmpl.startSizeZ, asset.startSizeZ);
    return &tmpl;
}

// 0x00661B20: DefaultUpdateModuleTemplate::copyFrom
DefaultUpdateModuleTemplate* copyDefaultUpdateModuleTemplate(
    DefaultUpdateModuleTemplate& dst,
    const DefaultUpdateModuleTemplate& src) noexcept {
    copyUpdateModuleTemplateBase(dst, src);
    dst.vtable = kDefaultUpdateModuleTemplateVtable;
    sim::copyRandomVariable(dst.startSizeX, src.startSizeX);
    sim::copyRandomVariable(dst.startSizeY, src.startSizeY);
    sim::copyRandomVariable(dst.startSizeZ, src.startSizeZ);
    return &dst;
}

// 0x0066A740: DefaultUpdateParticleModule::initialize
DefaultUpdateParticleModule* initializeDefaultUpdateParticle(
    DefaultUpdateParticleModule& module,
    FXParticle* particle,
    const DefaultUpdateModuleView& updateMod,
    float globalSizeMultiplier,
    sim::RandomState& rng) noexcept {
    const DefaultUpdateModuleTemplate* tmpl = updateMod.templatePtr;
    module.vtable = kDefaultUpdateParticleVtable;
    module.particle = particle;
    module.angleZ = clientRvFloat(tmpl->angleZ, rng);
    module.rotationType = tmpl->rotationType;

    const FXParticleSystemStub* sys = updateMod.systemRef->system;
#if defined(_MSC_VER) && defined(_M_IX86)
    const sim::RandomVariable* sysSizeVar = &sys->tmpl->systemSizeScaleRand;
    sim::RandomState* rngPtr = &rng;
    float sizeMulCopy = 0.0f;
    x87CopyFloat(sizeMulCopy, sys->sizeMultiplier);
    float baseSize = 0.0f;
    __asm {
        push rngPtr
        push sysSizeVar
        call sim::clientRandomVariableExtended
        add esp, 8
        fmul dword ptr [globalSizeMultiplier]
        fmul dword ptr [sizeMulCopy]
        fstp dword ptr [baseSize]
    }
    module.size = baseSize;

    x87CopyFloat(sizeMulCopy, sys->sizeMultiplier);
    const sim::RandomVariable* sizeRateVar = &tmpl->sizeRate;
    float rateVal = 0.0f;
    __asm {
        push rngPtr
        push sizeRateVar
        call sim::clientRandomVariableExtended
        add esp, 8
        fmul dword ptr [globalSizeMultiplier]
        fmul dword ptr [sizeMulCopy]
        fstp dword ptr [rateVal]
    }
    module.sizeRate = rateVal;
#else
    module.size = clientRvFloat(sys->tmpl->systemSizeScaleRand, rng) * globalSizeMultiplier * sys->sizeMultiplier;
    module.sizeRate = clientRvFloat(tmpl->sizeRate, rng) * globalSizeMultiplier * sys->sizeMultiplier;
#endif

    module.sizeRateDamping = clientRvFloat(tmpl->sizeRateDamping, rng);
    module.size = sAdd(sys->sizeBias, module.size);
    module.angularRateZ = clientRvFloat(tmpl->angularRateZ, rng);
    module.angularDamping = clientRvFloat(tmpl->startSizeZ, rng);
    module.xyRotationAngle = clientRvFloat(tmpl->startSizeX, rng);
    module.xyAngularRate = clientRvFloat(tmpl->startSizeY, rng);
    module.xyAngularDamping = clientRvFloat(tmpl->startSizeZ, rng);
    return &module;
}

// 0x0065EDF0: DefaultUpdateParticleModule::update
void updateDefaultUpdateParticle(DefaultUpdateParticleModule& module) noexcept {
    const float sizeRate = module.sizeRate;
    module.size = sAdd(module.size, sizeRate);
    module.sizeRate = sMul(module.sizeRateDamping, sizeRate);
    const float angularRateZ = module.angularRateZ;
    module.angleZ = sAdd(angularRateZ, module.angleZ);
    module.angularRateZ = sMul(module.angularDamping, angularRateZ);
    const float xyAngularRate = module.xyAngularRate;
    module.xyRotationAngle = sAdd(module.xyRotationAngle, xyAngularRate);
    module.xyAngularRate = sMul(module.xyAngularDamping, xyAngularRate);
}

// 0x0065F530: RenderObjectUpdateModuleTemplate::initializeFromAsset
RenderObjectUpdateModuleTemplate* initializeRenderObjectUpdateModuleTemplateFromAsset(
    RenderObjectUpdateModuleTemplate& tmpl,
    const RenderObjectUpdateModuleAssetData& asset) noexcept {
    tmpl.vtable = kRenderObjectUpdateTemplateVtable;
    sim::setRandomVariable(tmpl.sizeRate, 0.0f, 0.0f, 0);
    sim::setRandomVariable(tmpl.sizeRateDamping, 0.0f, 0.0f, 0);
    sim::setRandomVariable(tmpl.angleZ, 0.0f, 0.0f, 0);
    sim::setRandomVariable(tmpl.angularRateZ, 0.0f, 0.0f, 0);
    sim::setRandomVariable(tmpl.angularDamping, 0.0f, 0.0f, 0);

    sim::copyRandomVariable(tmpl.initialScaleX, asset.initialScaleX);
    sim::copyRandomVariable(tmpl.initialScaleY, asset.initialScaleY);
    sim::copyRandomVariable(tmpl.initialScaleZ, asset.initialScaleZ);
    sim::copyRandomVariable(tmpl.sizeRateY, asset.sizeRateY);
    sim::copyRandomVariable(tmpl.sizeRateZ, asset.sizeRateZ);
    sim::copyRandomVariable(tmpl.sizeDampingY, asset.sizeDampingY);
    sim::copyRandomVariable(tmpl.sizeDampingZ, asset.angleX);
    sim::copyRandomVariable(tmpl.angleX, asset.startSizeX);
    sim::copyRandomVariable(tmpl.angleY, asset.startSizeY);
    sim::copyRandomVariable(tmpl.angularRateX, asset.startSizeZ);
    sim::copyRandomVariable(tmpl.angularRateY, asset.sizeDampingZ);
    sim::copyRandomVariable(tmpl.sizeRate, asset.sizeRateX);
    sim::copyRandomVariable(tmpl.sizeRateDamping, asset.sizeDampingX);
    sim::copyRandomVariable(tmpl.angleZ, asset.angleZ);
    sim::copyRandomVariable(tmpl.angularRateZ, asset.angularRateZ);
    sim::copyRandomVariable(tmpl.angularDamping, asset.angularDamping);
    tmpl.rotationType = asset.rotationType;
    return &tmpl;
}

// 0x0065F690: RenderObjectUpdateModuleTemplate::copyFrom
RenderObjectUpdateModuleTemplate* copyRenderObjectUpdateModuleTemplate(
    RenderObjectUpdateModuleTemplate& dst,
    const RenderObjectUpdateModuleTemplate& src) noexcept {
    copyUpdateModuleTemplateBase(dst, src);
    dst.vtable = kRenderObjectUpdateTemplateVtable;
    dst.initialScaleX = src.initialScaleX;
    dst.initialScaleY = src.initialScaleY;
    dst.initialScaleZ = src.initialScaleZ;
    dst.sizeRateY = src.sizeRateY;
    dst.sizeRateZ = src.sizeRateZ;
    dst.sizeDampingY = src.sizeDampingY;
    dst.sizeDampingZ = src.sizeDampingZ;
    dst.angleX = src.angleX;
    dst.angleY = src.angleY;
    dst.angularRateX = src.angularRateX;
    dst.angularRateY = src.angularRateY;
    return &dst;
}

// 0x0066BAE0: RenderObjectParticleUpdateModule::initialize
RenderObjectParticleUpdateModule* initializeRenderObjectUpdateParticle(
    RenderObjectParticleUpdateModule& module,
    FXParticle* particle,
    const RenderObjectUpdateModuleView& updateMod,
    sim::RandomState& rng) noexcept {
    const RenderObjectUpdateModuleTemplate* tmpl = updateMod.templatePtr;
    module.vtable = kRenderObjectUpdateParticleVtable;
    module.particle = particle;

    const FXParticleSystemStub* sys = updateMod.systemRef->system;
    module.scale.x = clientRvFloat(tmpl->initialScaleX, rng);
#if defined(_MSC_VER) && defined(_M_IX86)
    float scaleYMul = 0.0f;
    x87CopyFloat(scaleYMul, sys->renderObjectScaleYMultiplier);
    const sim::RandomVariable* scaleYVar = &tmpl->initialScaleY;
    sim::RandomState* rngPtr = &rng;
    float scaleYVal = 0.0f;
    __asm {
        push rngPtr
        push scaleYVar
        call sim::clientRandomVariableExtended
        add esp, 8
        fmul dword ptr [scaleYMul]
        fstp dword ptr [scaleYVal]
    }
    module.scale.y = scaleYVal;
#else
    module.scale.y = clientRvFloat(tmpl->initialScaleY, rng) * sys->renderObjectScaleYMultiplier;
#endif
    module.scale.z = clientRvFloat(tmpl->initialScaleZ, rng);
    module.sizeRate.x = clientRvFloat(tmpl->sizeRate, rng);
    module.sizeRate.y = clientRvFloat(tmpl->sizeRateY, rng);
    module.sizeRate.z = clientRvFloat(tmpl->sizeRateZ, rng);
    module.sizeDamping.x = clientRvFloat(tmpl->sizeRateDamping, rng);
    module.sizeDamping.y = clientRvFloat(tmpl->sizeDampingY, rng);
    module.sizeDamping.z = clientRvFloat(tmpl->sizeDampingZ, rng);

    module.scale.x = sAdd(sys->sizeBias, module.scale.x);
    module.scale.y = sAdd(sys->sizeBias, module.scale.y);
    module.scale.z = sAdd(sys->sizeBias, module.scale.z);

    module.angleX = clientRvFloat(tmpl->angleX, rng);
    module.angleY = clientRvFloat(tmpl->angleY, rng);
    module.angleZ = clientRvFloat(tmpl->angleZ, rng);
    module.angularRate.x = clientRvFloat(tmpl->angularRateX, rng);
    module.angularRate.y = clientRvFloat(tmpl->angularRateY, rng);
    module.angularRate.z = clientRvFloat(tmpl->angularRateZ, rng);
    module.angularDamping = clientRvFloat(tmpl->angularDamping, rng);
    module.rotationType = tmpl->rotationType;
    return &module;
}

// 0x0065F900: RenderObjectParticleUpdateModule::update
void updateRenderObjectUpdateParticle(
    RenderObjectParticleUpdateModule& module) noexcept {
    const float rateX = module.sizeRate.x;
    const float rateY = module.sizeRate.y;
    const float rateZ = module.sizeRate.z;
    module.scale.x = sAdd(rateX, module.scale.x);
    module.scale.y = sAdd(module.scale.y, rateY);
    module.scale.z = sAdd(module.scale.z, rateZ);

    const float nextRateX = sMul(module.sizeDamping.x, rateX);
    module.sizeRate.y = sMul(module.sizeDamping.y, rateY);
    module.sizeRate.z = sMul(module.sizeDamping.z, rateZ);

    const float angRateX = module.angularRate.x;
    const float angRateY = module.angularRate.y;
    module.angleX = sAdd(module.angleX, angRateX);
    module.angleY = sAdd(angRateY, module.angleY);
    module.sizeRate.x = nextRateX;

    const float angRateZ = module.angularRate.z;
    module.angleZ = sAdd(angRateZ, module.angleZ);

    const float angDamp = module.angularDamping;
    module.angularRate.x = sMul(angDamp, angRateX);
    module.angularRate.y = sMul(angDamp, angRateY);
    module.angularRate.z = sMul(angDamp, angRateZ);
}

// 0x00666D70: SwarmPhysicsParticleModule::updatePhysics
void updateSwarmPhysicsParticle(SwarmPhysicsParticleModule& module) noexcept {
    FXParticle* particle = module.particle;
    const auto* sys = static_cast<const FXParticleSystemStub*>(particle->systemRef->system);
    const float dx = sSub(sys->worldTransform.m[3], particle->position.x);
    const float dy = sSub(sys->worldTransform.m[7], particle->position.y);
    const float dz = sSub(sys->worldTransform.m[11], particle->position.z);

    const float strength = module.attractionStrength;
    const float accelX = sMul(strength, dx);
    const float accelZ = sMul(strength, dz);
    const float accelY = sMul(strength, dy);

    const float velX = sAdd(particle->velocity.x, accelX);
    const float velZ = sAdd(particle->velocity.z, accelZ);
    particle->velocity.z = velZ;
    particle->velocity.x = velX;
    const float velY = sAdd(particle->velocity.y, accelY);
    particle->velocity.y = velY;

    const float posY = sAdd(particle->position.y, velY);
    const float posZ = sAdd(particle->position.z, velZ);
    const float posX = sAdd(particle->position.x, velX);
    particle->position.x = posX;
    particle->position.y = posY;
    particle->position.z = posZ;
}

// 0x0065F100: WavefrontEmissionVelocityModuleTemplate::initializeFromAsset
WavefrontEmissionVelocityModuleTemplate* initializeWavefrontVelocityTemplateFromAsset(
    WavefrontEmissionVelocityModuleTemplate& tmpl,
    const WavefrontVelocityAssetData& asset) noexcept {
    tmpl.vtable = kWavefrontVelocityTemplateVtable;
    tmpl.velocityType = 5u;
    tmpl.flag08 = asset.flag04;
    sim::copyRandomVariable(tmpl.var0, asset.var0);
    sim::copyRandomVariable(tmpl.var1, asset.var1);
    sim::copyRandomVariable(tmpl.var2, asset.var2);
    sim::copyRandomVariable(tmpl.var3, asset.var3);
    sim::copyRandomVariable(tmpl.var4, asset.var4);
    return &tmpl;
}

// 0x0065F160: WavefrontEmissionVelocityModuleTemplate::copyFrom
WavefrontEmissionVelocityModuleTemplate* copyWavefrontVelocityTemplate(
    WavefrontEmissionVelocityModuleTemplate& dst,
    const WavefrontEmissionVelocityModuleTemplate& src) noexcept {
    dst.velocityType = src.velocityType;
    dst.vtable = kWavefrontVelocityTemplateVtable;
    dst.flag08 = src.flag08;
    sim::copyRandomVariable(dst.var0, src.var0);
    sim::copyRandomVariable(dst.var1, src.var1);
    sim::copyRandomVariable(dst.var2, src.var2);
    sim::copyRandomVariable(dst.var3, src.var3);
    sim::copyRandomVariable(dst.var4, src.var4);
    return &dst;
}

// 0x0065F1E0: WavefrontEmissionVelocityModule::sampleParameters
void sampleWavefrontVelocityDimensions(
    const WavefrontEmissionVelocityModule& module,
    float& outA,
    float& outB,
    float& outC,
    float& outD,
    sim::RandomState& rng) noexcept {
    const WavefrontEmissionVelocityModuleTemplate* tmpl = module.templatePtr;
    outA = clientRvFloat(tmpl->var0, rng);
    outB = clientRvFloat(tmpl->var1, rng);
    outC = clientRvFloat(tmpl->var3, rng);
    outD = clientRvFloat(tmpl->var4, rng);
}

// 0x00662A00: TerrainFireEmissionModuleTemplate::initializeFromAsset
TerrainFireEmissionModuleTemplate* initializeTerrainFireTemplateFromAsset(
    TerrainFireEmissionModuleTemplate& tmpl,
    const TerrainFireAssetData& asset) noexcept {
    tmpl.volumeType = 6u;
    tmpl.isHollow = asset.isHollow;
    tmpl.vtable = kTerrainFireTemplateVtable;
    sim::copyRandomVariable(tmpl.xVar, asset.xVar);
    sim::copyRandomVariable(tmpl.yVar, asset.yVar);
    sim::copyRandomVariable(tmpl.zVar, asset.zVar);
    x87CopyFloat(tmpl.extraParam, asset.extraParam);
    return &tmpl;
}

// 0x00662990: TerrainFireEmissionModuleTemplate::copyFrom
TerrainFireEmissionModuleTemplate* copyTerrainFireTemplate(
    TerrainFireEmissionModuleTemplate& dst,
    const TerrainFireEmissionModuleTemplate& src) noexcept {
    dst.volumeType = src.volumeType;
    dst.isHollow = src.isHollow;
    dst.vtable = kTerrainFireTemplateVtable;
    sim::copyRandomVariable(dst.xVar, src.xVar);
    sim::copyRandomVariable(dst.yVar, src.yVar);
    sim::copyRandomVariable(dst.zVar, src.zVar);
    x87CopyFloat(dst.extraParam, src.extraParam);
    return &dst;
}

// 0x00662A60: TerrainFireEmissionModule::samplePoint
void sampleTerrainFirePoint(
    const TerrainFireEmissionModule& module,
    Vector3& outPoint,
    sim::RandomState& rng) noexcept {
    const TerrainFireEmissionModuleTemplate* tmpl = module.templatePtr;
    outPoint.x = clientRvFloat(tmpl->xVar, rng);
    outPoint.y = clientRvFloat(tmpl->yVar, rng);
    outPoint.z = clientRvFloat(tmpl->zVar, rng);
}

// 0x0065EF80: OrthoEmissionVelocityModuleTemplate::copyFrom
PointVelocityTemplateData* copyOrthoVelocityTemplate(
    PointVelocityTemplateData& dst,
    const PointVelocityTemplateData& src) noexcept {
    dst.type = src.type;
    dst.vtable = kOrthoVelocityTemplateVtable;
    sim::copyRandomVariable(dst.x, src.x);
    sim::copyRandomVariable(dst.y, src.y);
    sim::copyRandomVariable(dst.z, src.z);
    return &dst;
}

// 0x0065F040: OutwardEmissionVelocityModule::getVelocity
Vector3* computeOutwardEmissionVelocity(
    const OutwardEmissionVelocityModule& velocityMod,
    Vector3& outVel,
    const Vector3& emissionPos,
    const VolumeModuleView& volumeMod,
    sim::RandomState& rng) noexcept {
    const OutwardEmissionVelocityModuleTemplate* tmpl = velocityMod.templatePtr;
    const float radialSpeed = clientRvFloat(tmpl->radialSpeed, rng);
    const float normalSpeed = clientRvFloat(tmpl->normalSpeed, rng);

    if (volumeMod.vtable == 0x00BF49DCu) {
        computeCylinderOutwardVelocity(
            reinterpret_cast<const CylinderVolumeModuleView&>(volumeMod),
            outVel,
            emissionPos,
            radialSpeed,
            normalSpeed);
    } else if (volumeMod.vtable == 0x00BF4A2Cu) {
        computeLineOutwardVelocity(volumeMod, outVel, emissionPos, radialSpeed, normalSpeed);
    } else if (volumeMod.vtable == 0x00BF4CC4u) {
        computeSplineOutwardVelocity(
            reinterpret_cast<const SplineEmissionVolumeModule&>(volumeMod),
            outVel,
            emissionPos,
            radialSpeed,
            normalSpeed);
    } else {
        computeRadialOutwardVelocity(volumeMod, outVel, emissionPos, radialSpeed, normalSpeed);
    }
    return &outVel;
}

// 0x0065EF50: CylinderEmissionVolumeModule::stepRadius
void stepCylinderVolumeRadiusOffset(CylinderVolumeModuleView& module) noexcept {
    const auto* tmpl = static_cast<const CylinderEmissionVolumeData*>(module.data);
    const float nextRadius = sAdd(tmpl->radiusRate, module.currentRadius);
    module.currentRadius = nextRadius;
    if (kMinCylinderRadius > nextRadius) {
        x87CopyFloat(module.currentRadius, kMinCylinderRadius);
    }
}

namespace {

float x87InvSqrt(float val) noexcept {
    if (val == 0.0f) {
        return 0.0f;
    }
#if defined(_MSC_VER) && defined(_M_IX86)
    float res = 0.0f;
    __asm {
        fld dword ptr [val]
        fsqrt
        fld1
        fdivrp st(1), st(0)
        fstp dword ptr [res]
    }
    return res;
#else
    return 1.0f / std::sqrt(val);
#endif
}

} // namespace

// 0x00665420: CylinderEmissionVolumeModule::computeOutwardVelocity
void computeCylinderOutwardVelocity(
    const CylinderVolumeModuleView& /*module*/,
    Vector3& outVel,
    const Vector3& pos,
    float radialSpeed,
    float normalSpeed) noexcept {
    float nx = pos.x;
    float ny = pos.y;
    const float lenSq = sAdd(sMul(nx, nx), sMul(ny, ny));
    if (lenSq > 1.0e-4f) {
        const float invLen = x87InvSqrt(lenSq);
        nx = sMul(invLen, nx);
        ny = sMul(invLen, ny);
    }
    outVel.y = sMul(ny, radialSpeed);
    outVel.x = sMul(nx, radialSpeed);
    outVel.z = normalSpeed;
}

// 0x00665540: LineEmissionVolumeModule::computeOutwardVelocity
void computeLineOutwardVelocity(
    const VolumeModuleView& module,
    Vector3& outVel,
    const Vector3& /*pos*/,
    float radialSpeed,
    float normalSpeed) noexcept {
    const auto* tmpl = static_cast<const LineEmissionVolumeData*>(module.data);
    float dx = sSub(tmpl->end.x, tmpl->start.x);
    float dz = sSub(tmpl->end.z, tmpl->start.z);
    float dy = sSub(tmpl->end.y, tmpl->start.y);

    const float lenSq = sAdd(sAdd(sMul(dz, dz), sMul(dx, dx)), sMul(dy, dy));
    if (lenSq > 1.0e-4f) {
        const float invLen = x87InvSqrt(lenSq);
        dx = sMul(invLen, dx);
        dy = sMul(invLen, dy);
        dz = sMul(invLen, dz);
    }

    const float dzZero = sMul(dz, 0.0f);
    const float rx = sSub(dzZero, dy);
    const float ry = sSub(dx, dzZero);
    const float rz = sSub(sMul(dy, 0.0f), sMul(dx, 0.0f));

    const float nx = sMul(sSub(sMul(rz, dy), sMul(ry, dz)), normalSpeed);
    const float ny = sMul(sSub(sMul(rx, dz), sMul(rz, dx)), normalSpeed);
    const float nz = sMul(sSub(sMul(ry, dx), sMul(dy, rx)), normalSpeed);

    outVel.x = sAdd(sMul(rx, radialSpeed), nx);
    outVel.y = sAdd(sMul(ry, radialSpeed), ny);
    outVel.z = sAdd(sMul(rz, radialSpeed), nz);
}

// 0x006657D0: SplineEmissionVolumeModule::computeOutwardVelocity
Vector3* computeSplineOutwardVelocity(
    const SplineEmissionVolumeModule& module,
    Vector3& outVel,
    const Vector3& /*pos*/,
    float radialSpeed,
    float /*normalSpeed*/) noexcept {
    const float ty = module.tangent.y;
    const float tz = module.tangent.z;
    const float tx = module.tangent.x;

    const float tzZero = sMul(tz, 0.0f);
    const float vx = sSub(ty, tzZero);
    const float vy = sSub(tzZero, tx);
    const float vz = sSub(sMul(tx, 0.0f), sMul(ty, 0.0f));

    const float lenSq = sAdd(sAdd(sMul(vz, vz), sMul(vy, vy)), sMul(vx, vx));
    const float invLen = x87InvSqrt(lenSq);

    outVel.x = sMul(sMul(vx, invLen), radialSpeed);
    outVel.y = sMul(sMul(vy, invLen), radialSpeed);
    outVel.z = sMul(sMul(vz, invLen), radialSpeed);
    return &outVel;
}

// 0x00665B60: EmissionVolumeModule::computeRadialOutwardVelocity
Vector3* computeRadialOutwardVelocity(
    const VolumeModuleView& /*module*/,
    Vector3& outVel,
    const Vector3& pos,
    float radialSpeed,
    float /*normalSpeed*/) noexcept {
    const float py = pos.y;
    const float pz = pos.z;
    const float px = pos.x;

    const float lenSq = sAdd(sAdd(sMul(pz, pz), sMul(py, py)), sMul(px, px));
    const float invLen = x87InvSqrt(lenSq);

    outVel.x = sMul(sMul(px, invLen), radialSpeed);
    outVel.y = sMul(radialSpeed, sMul(py, invLen));
    outVel.z = sMul(sMul(pz, invLen), radialSpeed);
    return &outVel;
}

// 0x00662CA0: LightningDrawModuleTemplate::initializeFromAsset
LightningDrawModuleTemplate* initializeLightningDrawTemplateFromAsset(
    LightningDrawModuleTemplate& tmpl,
    const LightningDrawAssetData& asset) noexcept {
    initializeDefaultLightningDrawTemplate(tmpl);
    x87CopyFloat(tmpl.param04, asset.param04);
    x87CopyFloat(tmpl.param08, asset.param08);
    x87CopyFloat(tmpl.param0C, asset.param0C);
    x87CopyFloat(tmpl.param10, asset.param10);
    x87CopyFloat(tmpl.param14, asset.param14);
    x87CopyFloat(tmpl.param18, asset.param18);

    const LightningDrawAssetPair* pairs = asset.pairsPtr;
    for (int i = 0; i < 2; ++i) {
        sim::copyRandomVariable(tmpl.randomVars[0 + i], pairs[i].amplitude1);
        sim::copyRandomVariable(tmpl.randomVars[2 + i], pairs[i].amplitude2);
        sim::copyRandomVariable(tmpl.randomVars[4 + i], pairs[i].frequency1);
        sim::copyRandomVariable(tmpl.randomVars[6 + i], pairs[i].frequency2);
        sim::copyRandomVariable(tmpl.randomVars[8 + i], pairs[i].phase1);
        sim::copyRandomVariable(tmpl.randomVars[10 + i], pairs[i].phase2);
    }
    return &tmpl;
}

// 0x00662E10: LightningDrawModuleTemplate::copyFrom
LightningDrawModuleTemplate* copyLightningDrawTemplate(
    LightningDrawModuleTemplate& dst,
    const LightningDrawModuleTemplate& src) noexcept {
    dst.vtable = kLightningDrawTemplateVtable;
    x87CopyFloat(dst.param04, src.param04);
    x87CopyFloat(dst.param08, src.param08);
    x87CopyFloat(dst.param0C, src.param0C);
    x87CopyFloat(dst.param10, src.param10);
    x87CopyFloat(dst.param14, src.param14);
    x87CopyFloat(dst.param18, src.param18);
    for (int i = 0; i < 12; ++i) {
        dst.randomVars[i] = src.randomVars[i];
    }
    return &dst;
}

// 0x00662F50: LightningDrawModuleTemplate::initializeDefault
void initializeDefaultLightningDrawTemplate(
    LightningDrawModuleTemplate& tmpl) noexcept {
    tmpl.vtable = kLightningDrawTemplateVtable;
    tmpl.param04 = 0.0f;
    tmpl.param08 = 0.0f;
    tmpl.param0C = 0.0f;
    tmpl.param10 = 0.0f;
    tmpl.param14 = 0.0f;
    tmpl.param18 = 0.0f;
    for (int i = 0; i < 12; ++i) {
        sim::setRandomVariable(tmpl.randomVars[i], 0.0f, 0.0f, 1);
    }
}

// 0x0066AF10: LightningDrawModule::initialize
LightningDrawModule* initializeLightningDrawModule(
    LightningDrawModule& module,
    FXParticleSystemStub* const* systemRef,
    const LightningDrawModuleTemplate* tmpl,
    sim::RandomState& rng) noexcept {
    module.vtable = kLightningDrawModuleBaseVtable;
    FXParticleSystemStub* sys = *systemRef;
    module.system = sys;
    ++sys->refCount;
    module.vtable = kLightningDrawModuleVtable;
    module.templatePtr = tmpl;

    const FXParticleSystemStub* realSys =
        (sys->vtable != 0u && sys->tmpl == nullptr)
            ? reinterpret_cast<const FXParticleSystemStub*>(static_cast<std::uintptr_t>(sys->vtable))
            : sys;
    const std::uint32_t shaderType = (realSys != nullptr && realSys->tmpl != nullptr) ? realSys->tmpl->shaderType : 0u;
    switch (shaderType) {
        case 1u:
            module.shaderPassType = 1u;
            break;
        case 2u:
            module.shaderPassType = 2u;
            break;
        case 3u:
            module.shaderPassType = 0u;
            break;
        case 4u:
            module.shaderPassType = 3u;
            break;
        case 5u:
            module.shaderPassType = 4u;
            break;
        case 6u:
            module.shaderPassType = 5u;
            break;
        case 7u:
            module.shaderPassType = 6u;
            break;
        default:
            module.shaderPassType = 1u;
            break;
    }

    for (int i = 0; i < 2; ++i) {
        module.sampledValues[0 + i] = clientRvFloat(tmpl->randomVars[0 + i], rng);
#if defined(_MSC_VER) && defined(_M_IX86)
        sim::RandomState* rngPtr = &rng;
        const sim::RandomVariable* rv2 = &tmpl->randomVars[2 + i];
        const float v0 = module.sampledValues[0 + i];
        float v2 = 0.0f;
        __asm {
            push rngPtr
            push rv2
            call sim::clientRandomVariableExtended
            add esp, 8
            fadd dword ptr [v0]
            fstp dword ptr [v2]
        }
        module.sampledValues[2 + i] = v2;
#else
        module.sampledValues[2 + i] = clientRvFloat(tmpl->randomVars[2 + i], rng) + module.sampledValues[0 + i];
#endif

        module.sampledValues[4 + i] = clientRvFloat(tmpl->randomVars[4 + i], rng);
#if defined(_MSC_VER) && defined(_M_IX86)
        const sim::RandomVariable* rv6 = &tmpl->randomVars[6 + i];
        const float v4 = module.sampledValues[4 + i];
        float v6 = 0.0f;
        __asm {
            push rngPtr
            push rv6
            call sim::clientRandomVariableExtended
            add esp, 8
            fadd dword ptr [v4]
            fstp dword ptr [v6]
        }
        module.sampledValues[6 + i] = v6;

        const sim::RandomVariable* rv8 = &tmpl->randomVars[8 + i];
        float v8 = 0.0f;
        __asm {
            push rngPtr
            push rv8
            call sim::clientRandomVariableExtended
            add esp, 8
            fchs
            fstp dword ptr [v8]
        }
        module.sampledValues[8 + i] = v8;

        const sim::RandomVariable* rv10 = &tmpl->randomVars[10 + i];
        float v10 = 0.0f;
        __asm {
            push rngPtr
            push rv10
            call sim::clientRandomVariableExtended
            add esp, 8
            fsubr dword ptr [v8]
            fstp dword ptr [v10]
        }
        module.sampledValues[10 + i] = v10;
#else
        module.sampledValues[6 + i] = clientRvFloat(tmpl->randomVars[6 + i], rng) + module.sampledValues[4 + i];
        module.sampledValues[8 + i] = -clientRvFloat(tmpl->randomVars[8 + i], rng);
        module.sampledValues[10 + i] = module.sampledValues[8 + i] - clientRvFloat(tmpl->randomVars[10 + i], rng);
#endif
    }
    return &module;
}

// 0x00662B60: LightningDrawModule::packParticleVertices
void packLightningParticleVertices(
    const LightningVertexPackerModule& module,
    const FXParticle& particle,
    LightningSwarmVertex* outVertices) noexcept {
    float initLifeFloat = 0.0f;
    float ageFloat = 0.0f;
#if defined(_MSC_VER) && defined(_M_IX86)
    const std::int32_t signedLife = static_cast<std::int32_t>(particle.initialLifetime);
    const float deathTs = particle.deathTimestamp;
    __asm {
        fild dword ptr [signedLife]
        mov eax, signedLife
        test eax, eax
        jge life_nonneg
        fadd dword ptr [kTwoPow32]
    life_nonneg:
        fst dword ptr [initLifeFloat]
        fsubr dword ptr [deathTs]
        fstp dword ptr [ageFloat]
    }
#else
    initLifeFloat = static_cast<float>(particle.initialLifetime);
    ageFloat = particle.deathTimestamp - initLifeFloat;
#endif
    const float seedPhase = particle.randomSeedPhase;
    if (module.segmentCount <= 0) {
        return;
    }
    for (std::int32_t i = 0; i < module.segmentCount; ++i) {
        LightningSwarmVertex& v = outVertices[i];
        x87CopyFloat(v.position.x, particle.position.x);
        x87CopyFloat(v.position.y, particle.position.y);
        x87CopyFloat(v.position.z, particle.position.z);
        v.initialLifetime = initLifeFloat;
        x87CopyFloat(v.velocity.x, particle.velocity.x);
        x87CopyFloat(v.velocity.y, particle.velocity.y);
        x87CopyFloat(v.velocity.z, particle.velocity.z);
        v.age = ageFloat;
        v.randomSeedPhase = seedPhase;
        v.segmentIndex = static_cast<float>(i);
    }
}

// 0x0065F460: RenderObjectDrawModuleTemplate::copyFrom
RenderObjectDrawModuleTemplate* copyRenderObjectDrawTemplate(
    RenderObjectDrawModuleTemplate& dst,
    const RenderObjectDrawModuleTemplate& src) noexcept {
    dst.vtable = kRenderObjectDrawTemplateVtable;
    dst.flag04 = src.flag04;
    x87CopyFloat(dst.param08, src.param08);
    dst.flag0C = src.flag0C;
    dst.entry0[0] = src.entry0[0];
    dst.entry0[1] = src.entry0[1];
    dst.entry0[2] = src.entry0[2];
    x87CopyFloat(dst.scale0, src.scale0);
    dst.entry1[0] = src.entry1[0];
    dst.entry1[1] = src.entry1[1];
    dst.entry1[2] = src.entry1[2];
    x87CopyFloat(dst.scale1, src.scale1);
    dst.entry2[0] = src.entry2[0];
    dst.entry2[1] = src.entry2[1];
    dst.entry2[2] = src.entry2[2];
    x87CopyFloat(dst.scale2, src.scale2);
    return &dst;
}

// 0x0066AE60: LifeEventModule::initialize
LifeEventModule* initializeLifeEventModule(
    LifeEventModule& module,
    FXParticleSystemStub* const* systemRef,
    const LifeEventModuleTemplate* tmpl,
    sim::RandomState& rng) noexcept {
    module.vtable = kLifeEventModuleBaseVtable;
    FXParticleSystemStub* sys = *systemRef;
    module.system = sys;
    ++sys->refCount;
    module.vtable = kLifeEventModuleVtable;
    module.templatePtr = tmpl;

#if defined(_MSC_VER) && defined(_M_IX86)
    const sim::RandomVariable* rvPtr = &tmpl->eventTimeRand;
    sim::RandomState* rngPtr = &rng;
    std::uint16_t savedCw = 0;
    std::uint16_t chopCw = 0;
    std::int64_t truncated = 0;
    __asm {
        push rngPtr
        push rvPtr
        call sim::clientRandomVariableExtended
        add esp, 8
        fnstcw word ptr [savedCw]
        movzx eax, word ptr [savedCw]
        or ah, 0Ch
        mov word ptr [chopCw], ax
        fldcw word ptr [chopCw]
        fistp qword ptr [truncated]
        fldcw word ptr [savedCw]
    }
    module.triggerFrame = static_cast<std::int32_t>(truncated);
#else
    module.triggerFrame = static_cast<std::int32_t>(clientRvFloat(tmpl->eventTimeRand, rng));
#endif
    module.active = 1u;
    return &module;
}

// 0x006699D0: FXParticleBase::initialize
void initializeParticleBase(
    FXParticle& particle,
    FXParticleSystemRef* const* systemRefPtr,
    const FXParticleSpawnInfo& spawnInfo,
    std::uint32_t& activeParticleCounter) noexcept {
    particle.vtable = kFXParticleBaseVtable;
    particle.velocity = spawnInfo.velocity;
    particle.position = spawnInfo.position;
    particle.emitterPosition = spawnInfo.emitterPosition;
    particle.initialLifetime = spawnInfo.lifetime;
    particle.spawnFrame = spawnInfo.spawnFrame;
    particle.worldSpace = spawnInfo.worldSpace;
    particle.systemRef = *systemRefPtr;
    ++particle.systemRef->refCount;
    particle.remainingLifetime = spawnInfo.lifetime;
    particle.activeFlag = 1u;
    particle.deadFlag = 0u;
    ++activeParticleCounter;
}

// 0x0066ABF0: FXParticle::initialize (gameclient\system\FXParticleSystem\FxpsGPUStorageModule.cpp)
FXParticle* initializeParticle(
    FXParticle& particle,
    FXParticleSystemRef* const* systemRefPtr,
    const FXParticleSpawnInfo& spawnInfo,
    std::uint32_t& activeParticleCounter,
    std::uint32_t currentClientFrame,
    float frameDurationSeconds,
    sim::RandomState& rng) noexcept {
    initializeParticleBase(particle, systemRefPtr, spawnInfo, activeParticleCounter);
    particle.vtable = kFXParticleVtable;
    particle.randomSeedPhase = clientRandomRealFloat(rng, 0.0f, 15.0f);

#if defined(_MSC_VER) && defined(_M_IX86)
    const std::int32_t signedFrame = static_cast<std::int32_t>(currentClientFrame);
    const std::int32_t signedLife = static_cast<std::int32_t>(particle.initialLifetime);
    float deathTs = 0.0f;
    __asm {
        fild dword ptr [signedFrame]
        mov edx, signedFrame
        test edx, edx
        jge frame_nonneg
        fadd dword ptr [kTwoPow32]
    frame_nonneg:
        fmul dword ptr [frameDurationSeconds]
        fild dword ptr [signedLife]
        mov eax, signedLife
        test eax, eax
        jge life_nonneg2
        fadd dword ptr [kTwoPow32]
    life_nonneg2:
        faddp st(1), st(0)
        fstp dword ptr [deathTs]
    }
    particle.deathTimestamp = deathTs;
#else
    particle.deathTimestamp =
        static_cast<float>(currentClientFrame) * frameDurationSeconds + static_cast<float>(particle.initialLifetime);
#endif

    particle.colorParticleModule = nullptr;
    particle.alphaParticleModule = nullptr;
    particle.updateParticleModule = nullptr;
    return &particle;
}

// 0x00661030: evaluateCubicHermiteSpline
void evaluateCubicHermiteSpline(
    Vector3& outPoint,
    float t,
    const Vector3& p0,
    const Vector3& p1,
    const Vector3& m0,
    const Vector3& m1) noexcept {
    const float t2 = sMul(t, t);
    const float t3 = sMul(t2, t);
    const float threeT2 = sMul(t2, 3.0f);
    const float h00 = sAdd(sSub(sMul(t3, 2.0f), threeT2), 1.0f);

    const float p0y = sMul(p0.y, h00);
    const float p0z = sMul(p0.z, h00);
    const float p0x = sMul(p0.x, h00);

    const float h10 = sAdd(sSub(t3, sMul(t2, 2.0f)), t);
    const float accX = sAdd(sMul(m0.x, h10), p0x);
    const float accY = sAdd(sMul(m0.y, h10), p0y);
    const float accZ = sAdd(sMul(m0.z, h10), p0z);

    const float h11 = sSub(t3, t2);
    const float h01 = sAdd(sMul(t3, -2.0f), threeT2);

    const float m1x = sMul(m1.x, h11);
    const float m1y = sMul(m1.y, h11);
    const float m1z = sMul(m1.z, h11);

    const float p1y = sMul(p1.y, h01);
    const float p1x = sMul(p1.x, h01);
    const float p1z = sMul(p1.z, h01);

    outPoint.x = sAdd(sAdd(accX, m1x), p1x);
    outPoint.y = sAdd(sAdd(accY, m1y), p1y);
    outPoint.z = sAdd(sAdd(accZ, m1z), p1z);
}

// 0x00664FA0: inverseTransformPointByMatrix34
void inverseTransformPointByMatrix34(
    const Matrix34& mat,
    const Vector3& inPoint,
    Vector3& outPoint) noexcept {
    const float dx = sSub(inPoint.x, mat.m[3]);
    const float dy = sSub(inPoint.y, mat.m[7]);
    const float dz = sSub(inPoint.z, mat.m[11]);

    const float ox = sAdd(sAdd(sMul(mat.m[4], dy), sMul(mat.m[8], dz)), sMul(mat.m[0], dx));
    const float oy = sAdd(sAdd(sMul(mat.m[1], dx), sMul(mat.m[5], dy)), sMul(mat.m[9], dz));
    const float oz = sAdd(sAdd(sMul(mat.m[2], dx), sMul(mat.m[6], dy)), sMul(mat.m[10], dz));

    outPoint.x = ox;
    outPoint.y = oy;
    outPoint.z = oz;
}

// 0x00660D50: Matrix34::multiply
void multiplyMatrix34(
    Matrix34& outMat,
    const Matrix34& lhs,
    const Matrix34& rhs) noexcept {
    for (int col = 0; col < 4; ++col) {
        const float b0 = rhs.m[0 + col];
        const float b1 = rhs.m[4 + col];
        const float b2 = rhs.m[8 + col];

        const float r0 = sAdd(sAdd(sMul(lhs.m[0], b0), sMul(b2, lhs.m[2])), sMul(b1, lhs.m[1]));
        float r1 = 0.0f;
        if (col == 2) {
            r1 = sAdd(sAdd(sMul(lhs.m[5], b1), sMul(lhs.m[6], b2)), sMul(lhs.m[4], b0));
        } else {
            r1 = sAdd(sAdd(sMul(lhs.m[6], b2), sMul(lhs.m[5], b1)), sMul(lhs.m[4], b0));
        }
        const float r2 = sAdd(sAdd(sMul(lhs.m[10], b2), sMul(lhs.m[9], b1)), sMul(lhs.m[8], b0));

        if (col == 3) {
            outMat.m[0 + col] = sAdd(r0, lhs.m[3]);
            outMat.m[4 + col] = sAdd(r1, lhs.m[7]);
            outMat.m[8 + col] = sAdd(r2, lhs.m[11]);
        } else {
            outMat.m[0 + col] = r0;
            outMat.m[4 + col] = r1;
            outMat.m[8 + col] = r2;
        }
    }
}

// 0x00660F80: extractTransposeRotation33
void extractTransposeRotation33(
    Matrix33& outRot,
    const Matrix34& inMat) noexcept {
    outRot.m[0] = inMat.m[0];
    outRot.m[1] = inMat.m[4];
    outRot.m[2] = inMat.m[8];
    outRot.m[3] = inMat.m[1];
    outRot.m[4] = inMat.m[5];
    outRot.m[5] = inMat.m[9];
    outRot.m[6] = inMat.m[2];
    outRot.m[7] = inMat.m[6];
    outRot.m[8] = inMat.m[10];
}

// 0x00669060: buildEulerXYZMatrix33
void buildEulerXYZMatrix33(
    Matrix33& outRot,
    const Vector3& eulerAngles) noexcept {
    float sinX = 0.0f;
    float cosX = 0.0f;
    float sinY = 0.0f;
    float cosY = 0.0f;
    float sinZ = 0.0f;
    float cosZ = 0.0f;
    float m00 = 0.0f;
    float m01 = 0.0f;
#if defined(_MSC_VER) && defined(_M_IX86)
    const float ex = eulerAngles.x;
    const float ey = eulerAngles.y;
    const float ez = eulerAngles.z;
    __asm {
        fld dword ptr [ex]
        fld st(0)
        fsin
        fstp dword ptr [sinX]
        fcos
        fstp dword ptr [cosX]
        fld dword ptr [ey]
        fld st(0)
        fsin
        fstp dword ptr [sinY]
        fcos
        fst dword ptr [cosY]
        fld dword ptr [ez]
        fld st(0)
        fsin
        fst dword ptr [sinZ]
        fxch st(1)
        fcos
        fst dword ptr [cosZ]
        fmul st(0), st(2)
        fstp dword ptr [m00]
        fmulp st(1), st(0)
        fstp dword ptr [m01]
    }
#else
    sinX = std::sin(eulerAngles.x);
    cosX = std::cos(eulerAngles.x);
    sinY = std::sin(eulerAngles.y);
    cosY = std::cos(eulerAngles.y);
    sinZ = std::sin(eulerAngles.z);
    cosZ = std::cos(eulerAngles.z);
    m00 = cosZ * cosY;
    m01 = sinZ * cosY;
#endif
    outRot.m[0] = m00;
    outRot.m[1] = m01;
    outRot.m[2] = sSub(-0.0f, sinY);

    const float szSx = sMul(sinZ, sinX);
    const float szCx = sMul(sinZ, cosX);
    const float czSx = sMul(cosZ, sinX);
    const float czCx = sMul(cosZ, cosX);

    outRot.m[3] = sSub(sMul(czSx, sinY), szCx);
    outRot.m[4] = sAdd(sMul(szSx, sinY), czCx);
    outRot.m[5] = sMul(cosY, sinX);
    outRot.m[6] = sAdd(sMul(czCx, sinY), szSx);
    outRot.m[7] = sSub(sMul(szCx, sinY), czSx);
    outRot.m[8] = sMul(cosY, cosX);
}

// 0x00669150: transposeMatrix33
void transposeMatrix33(
    Matrix33& outRot,
    const Matrix33& inRot) noexcept {
    x87CopyFloat(outRot.m[0], inRot.m[0]);
    x87CopyFloat(outRot.m[1], inRot.m[3]);
    x87CopyFloat(outRot.m[2], inRot.m[6]);
    x87CopyFloat(outRot.m[3], inRot.m[1]);
    x87CopyFloat(outRot.m[4], inRot.m[4]);
    x87CopyFloat(outRot.m[5], inRot.m[7]);
    x87CopyFloat(outRot.m[6], inRot.m[2]);
    x87CopyFloat(outRot.m[7], inRot.m[5]);
    x87CopyFloat(outRot.m[8], inRot.m[8]);
}

} // namespace ra3::gameclient
