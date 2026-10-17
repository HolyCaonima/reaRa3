#include "../Targets.h"

#include <ra3/gameclient/ParticleModules65.h>

#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

using ra3::gameclient::CylinderVolumeModuleView;
using ra3::gameclient::DefaultAlphaParticleModule;
using ra3::gameclient::DefaultColorModule;
using ra3::gameclient::DefaultColorParticleModule;
using ra3::gameclient::Matrix33;
using ra3::gameclient::Matrix34;
using ra3::gameclient::PointVelocityTemplateData;
using ra3::gameclient::RenderObjectDrawModuleTemplate;
using ra3::gameclient::SplineEmissionVolumeModule;
using ra3::gameclient::Vector3;
using ra3::gameclient::VolumeModuleView;
using ra3::gameclient::WavefrontEmissionVelocityModuleTemplate;

TargetState g_alphaRate;
TargetState g_colorTint;
TargetState g_colorRate;
TargetState g_cylStepRadius;
TargetState g_orthoTmplCopy;
TargetState g_waveTmplCopy;
TargetState g_roDrawTmplCopy;
TargetState g_mulMat34;
TargetState g_extractRot33;
TargetState g_invMat34Pt;
TargetState g_cylOutwardVel;
TargetState g_splineOutwardVel;
TargetState g_radialOutwardVel;
TargetState g_eulerMat33;
TargetState g_transposeMat33;

void __fastcall alphaRateWrapper(DefaultAlphaParticleModule* module, void*)
{
    TargetState& state = g_alphaRate;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    DefaultAlphaParticleModule ours = *module;
    ra3::gameclient::computeDefaultAlphaRate(ours);
    if (state.mode == Mode::replace) {
        *module = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    reinterpret_cast<void(__thiscall*)(DefaultAlphaParticleModule*)>(
        state.original)(module);
    recordOutput("fxpsalpharate", state, module, &ours, sizeof(ours));
}

void __fastcall colorTintWrapper(
    DefaultColorModule* module, void*, std::uint32_t packedRgb)
{
    TargetState& state = g_colorTint;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    DefaultColorModule ours = *module;
    ra3::gameclient::applyDefaultColorTint(ours, packedRgb);
    if (state.mode == Mode::replace) {
        *module = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    reinterpret_cast<void(__thiscall*)(DefaultColorModule*, std::uint32_t)>(
        state.original)(module, packedRgb);
    recordOutput("fxpscolortint", state, module, &ours, sizeof(ours));
}

void __fastcall colorRateWrapper(DefaultColorParticleModule* module, void*)
{
    TargetState& state = g_colorRate;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    DefaultColorParticleModule ours = *module;
    ra3::gameclient::computeDefaultColorRate(ours);
    if (state.mode == Mode::replace) {
        *module = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    reinterpret_cast<void(__thiscall*)(DefaultColorParticleModule*)>(
        state.original)(module);
    recordOutput("fxpscolorrate", state, module, &ours, sizeof(ours));
}

void __fastcall cylStepRadiusWrapper(CylinderVolumeModuleView* module, void*)
{
    TargetState& state = g_cylStepRadius;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    CylinderVolumeModuleView ours = *module;
    ra3::gameclient::stepCylinderVolumeRadiusOffset(ours);
    if (state.mode == Mode::replace) {
        *module = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    reinterpret_cast<void(__thiscall*)(CylinderVolumeModuleView*)>(
        state.original)(module);
    recordOutput(
        "fxpscylstepradius", state, &module->currentRadius,
        &ours.currentRadius, sizeof(ours.currentRadius));
}

PointVelocityTemplateData* __fastcall orthoTmplCopyWrapper(
    PointVelocityTemplateData* dst, void*, const PointVelocityTemplateData* src)
{
    TargetState& state = g_orthoTmplCopy;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const PointVelocityTemplateData srcCopy = *src;
    PointVelocityTemplateData ours = *dst;
    ra3::gameclient::copyOrthoVelocityTemplate(ours, srcCopy);
    if (state.mode == Mode::replace) {
        *dst = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return dst;
    }
    reinterpret_cast<PointVelocityTemplateData*(__thiscall*)(
        PointVelocityTemplateData*, const PointVelocityTemplateData*)>(
            state.original)(dst, src);
    recordOutput("fxpsorthotmplcopy", state, dst, &ours, sizeof(ours));
    return dst;
}

WavefrontEmissionVelocityModuleTemplate* __fastcall waveTmplCopyWrapper(
    WavefrontEmissionVelocityModuleTemplate* dst, void*,
    const WavefrontEmissionVelocityModuleTemplate* src)
{
    TargetState& state = g_waveTmplCopy;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const WavefrontEmissionVelocityModuleTemplate srcCopy = *src;
    WavefrontEmissionVelocityModuleTemplate ours = *dst;
    ra3::gameclient::copyWavefrontVelocityTemplate(ours, srcCopy);
    if (state.mode == Mode::replace) {
        *dst = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return dst;
    }
    reinterpret_cast<WavefrontEmissionVelocityModuleTemplate*(__thiscall*)(
        WavefrontEmissionVelocityModuleTemplate*,
        const WavefrontEmissionVelocityModuleTemplate*)>(
            state.original)(dst, src);
    recordOutput("fxpswavetmplcopy", state, dst, &ours, sizeof(ours));
    return dst;
}

RenderObjectDrawModuleTemplate* __fastcall roDrawTmplCopyWrapper(
    RenderObjectDrawModuleTemplate* dst, void*,
    const RenderObjectDrawModuleTemplate* src)
{
    TargetState& state = g_roDrawTmplCopy;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const RenderObjectDrawModuleTemplate srcCopy = *src;
    RenderObjectDrawModuleTemplate ours = *dst;
    ra3::gameclient::copyRenderObjectDrawTemplate(ours, srcCopy);
    if (state.mode == Mode::replace) {
        *dst = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return dst;
    }
    reinterpret_cast<RenderObjectDrawModuleTemplate*(__thiscall*)(
        RenderObjectDrawModuleTemplate*,
        const RenderObjectDrawModuleTemplate*)>(
            state.original)(dst, src);
    recordOutput("fxpsrodrawtmplcopy", state, dst, &ours, sizeof(ours));
    return dst;
}

void __fastcall mulMat34Wrapper(
    Matrix34* outMat, void*, const Matrix34* lhs, const Matrix34* rhs)
{
    TargetState& state = g_mulMat34;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const Matrix34 lhsCopy = *lhs;
    const Matrix34 rhsCopy = *rhs;
    Matrix34 ours{};
    ra3::gameclient::multiplyMatrix34(ours, lhsCopy, rhsCopy);
    if (state.mode == Mode::replace) {
        *outMat = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    reinterpret_cast<void(__thiscall*)(
        Matrix34*, const Matrix34*, const Matrix34*)>(
            state.original)(outMat, lhs, rhs);
    recordOutput("fxpsmulmat34", state, outMat, &ours, sizeof(ours));
}

void __cdecl extractRot33Wrapper(Matrix33* outRot, const Matrix34* inMat)
{
    TargetState& state = g_extractRot33;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const Matrix34 inCopy = *inMat;
    Matrix33 ours{};
    ra3::gameclient::extractTransposeRotation33(ours, inCopy);
    if (state.mode == Mode::replace) {
        *outRot = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    reinterpret_cast<void(__cdecl*)(Matrix33*, const Matrix34*)>(
        state.original)(outRot, inMat);
    recordOutput("fxpsextractrot33", state, outRot, &ours, sizeof(ours));
}

void __cdecl invMat34PtWrapper(
    const Matrix34* mat, const Vector3* inPoint, Vector3* outPoint)
{
    TargetState& state = g_invMat34Pt;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const Matrix34 matCopy = *mat;
    const Vector3 inCopy = *inPoint;
    Vector3 ours{};
    ra3::gameclient::inverseTransformPointByMatrix34(matCopy, inCopy, ours);
    if (state.mode == Mode::replace) {
        *outPoint = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    reinterpret_cast<void(__cdecl*)(
        const Matrix34*, const Vector3*, Vector3*)>(
            state.original)(mat, inPoint, outPoint);
    recordOutput("fxpsinvmat34pt", state, outPoint, &ours, sizeof(ours));
}

void __fastcall cylOutwardVelWrapper(
    const CylinderVolumeModuleView* module, void*, Vector3* outVel,
    const Vector3* pos, float radialSpeed, float normalSpeed)
{
    TargetState& state = g_cylOutwardVel;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const Vector3 posCopy = *pos;
    Vector3 ours{};
    ra3::gameclient::computeCylinderOutwardVelocity(
        *module, ours, posCopy, radialSpeed, normalSpeed);
    if (state.mode == Mode::replace) {
        *outVel = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    reinterpret_cast<void(__thiscall*)(
        const CylinderVolumeModuleView*, Vector3*, const Vector3*, float, float)>(
            state.original)(module, outVel, pos, radialSpeed, normalSpeed);
    recordOutput("fxpscyloutwardvel", state, outVel, &ours, sizeof(ours));
}

Vector3* __fastcall splineOutwardVelWrapper(
    const SplineEmissionVolumeModule* module, void*, Vector3* outVel,
    const Vector3* pos, float radialSpeed, float normalSpeed)
{
    TargetState& state = g_splineOutwardVel;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const Vector3 posCopy = *pos;
    Vector3 ours{};
    ra3::gameclient::computeSplineOutwardVelocity(
        *module, ours, posCopy, radialSpeed, normalSpeed);
    if (state.mode == Mode::replace) {
        *outVel = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return outVel;
    }
    reinterpret_cast<Vector3*(__thiscall*)(
        const SplineEmissionVolumeModule*, Vector3*, const Vector3*, float, float)>(
            state.original)(module, outVel, pos, radialSpeed, normalSpeed);
    recordOutput("fxpssplineoutwardvel", state, outVel, &ours, sizeof(ours));
    return outVel;
}

Vector3* __fastcall radialOutwardVelWrapper(
    const VolumeModuleView* module, void*, Vector3* outVel,
    const Vector3* pos, float radialSpeed, float normalSpeed)
{
    TargetState& state = g_radialOutwardVel;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const Vector3 posCopy = *pos;
    Vector3 ours{};
    ra3::gameclient::computeRadialOutwardVelocity(
        *module, ours, posCopy, radialSpeed, normalSpeed);
    if (state.mode == Mode::replace) {
        *outVel = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return outVel;
    }
    reinterpret_cast<Vector3*(__thiscall*)(
        const VolumeModuleView*, Vector3*, const Vector3*, float, float)>(
            state.original)(module, outVel, pos, radialSpeed, normalSpeed);
    recordOutput("fxpsradialoutwardvel", state, outVel, &ours, sizeof(ours));
    return outVel;
}

void __cdecl eulerMat33Wrapper(Matrix33* outRot, const Vector3* eulerAngles)
{
    TargetState& state = g_eulerMat33;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const Vector3 eulerCopy = *eulerAngles;
    Matrix33 ours{};
    ra3::gameclient::buildEulerXYZMatrix33(ours, eulerCopy);
    if (state.mode == Mode::replace) {
        *outRot = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    reinterpret_cast<void(__cdecl*)(Matrix33*, const Vector3*)>(
        state.original)(outRot, eulerAngles);
    recordOutput("fxpseulermat33", state, outRot, &ours, sizeof(ours));
}

void __cdecl transposeMat33Wrapper(Matrix33* outRot, const Matrix33* inRot)
{
    TargetState& state = g_transposeMat33;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const Matrix33 inCopy = *inRot;
    Matrix33 ours{};
    ra3::gameclient::transposeMatrix33(ours, inCopy);
    if (state.mode == Mode::replace) {
        *outRot = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    reinterpret_cast<void(__cdecl*)(Matrix33*, const Matrix33*)>(
        state.original)(outRot, inRot);
    recordOutput("fxpstransposemat33", state, outRot, &ours, sizeof(ours));
}

}  // namespace

void appendParticleModules65Targets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"fxpsalpharate", 0x0025E1E0,
         {0x83, 0xEC, 0x10, 0x8B, 0x51, 0x50, 0xF3, 0x0F,
          0x10, 0x4C, 0xD1, 0x0C, 0x0F, 0x57, 0xC0, 0x0F},
         6, reinterpret_cast<void*>(&alphaRateWrapper),
         &g_alphaRate, true},
        {"fxpscolortint", 0x0025E490,
         {0x83, 0xEC, 0x0C, 0x80, 0xB9, 0x9C, 0x00, 0x00,
          0x00, 0x00, 0x0F, 0x84, 0xF5, 0x01, 0x00, 0x00},
         10, reinterpret_cast<void*>(&colorTintWrapper),
         &g_colorTint, true},
        {"fxpscolorrate", 0x0025E710,
         {0x83, 0xEC, 0x10, 0x0F, 0x57, 0xC0, 0x56, 0x8B,
          0xB1, 0xA0, 0x00, 0x00, 0x00, 0x8B, 0xC6, 0xC1},
         6, reinterpret_cast<void*>(&colorRateWrapper),
         &g_colorRate, true},
        {"fxpscylstepradius", 0x0025EF50,
         {0x8B, 0x41, 0x08, 0xF3, 0x0F, 0x10, 0x40, 0x10,
          0xF3, 0x0F, 0x58, 0x41, 0x0C, 0xF3, 0x0F, 0x10},
         8, reinterpret_cast<void*>(&cylStepRadiusWrapper),
         &g_cylStepRadius, true},
        {"fxpsorthotmplcopy", 0x0025EF80,
         {0x8B, 0xC1, 0x8B, 0x4C, 0x24, 0x04, 0xC7, 0x00,
          0x60, 0x2A, 0xBF, 0x00, 0x8B, 0x51, 0x04, 0x89},
         6, reinterpret_cast<void*>(&orthoTmplCopyWrapper),
         &g_orthoTmplCopy, true},
        {"fxpswavetmplcopy", 0x0025F160,
         {0x8B, 0xC1, 0x8B, 0x4C, 0x24, 0x04, 0xC7, 0x00,
          0x60, 0x2A, 0xBF, 0x00, 0x8B, 0x51, 0x04, 0x89},
         6, reinterpret_cast<void*>(&waveTmplCopyWrapper),
         &g_waveTmplCopy, true},
        {"fxpsrodrawtmplcopy", 0x0025F460,
         {0x8B, 0xC1, 0x8B, 0x4C, 0x24, 0x04, 0xC7, 0x00,
          0xE0, 0x30, 0xBF, 0x00, 0x0F, 0xB6, 0x51, 0x04},
         6, reinterpret_cast<void*>(&roDrawTmplCopyWrapper),
         &g_roDrawTmplCopy, true},
        {"fxpsmulmat34", 0x00260D50,
         {0x8B, 0x44, 0x24, 0x04, 0x8B, 0x54, 0x24, 0x08,
          0xF3, 0x0F, 0x10, 0x52, 0x20, 0xF3, 0x0F, 0x10},
         8, reinterpret_cast<void*>(&mulMat34Wrapper),
         &g_mulMat34, true},
        {"fxpsextractrot33", 0x00260F80,
         {0x83, 0xEC, 0x24, 0x8B, 0x44, 0x24, 0x2C, 0xF3,
          0x0F, 0x10, 0x40, 0x08, 0xF3, 0x0F, 0x11, 0x44},
         7, reinterpret_cast<void*>(&extractRot33Wrapper),
         &g_extractRot33, true},
        {"fxpsinvmat34pt", 0x00264FA0,
         {0x8B, 0x54, 0x24, 0x0C, 0x8B, 0x4C, 0x24, 0x08,
          0x83, 0xEC, 0x18, 0x3B, 0xD1, 0x75, 0x18, 0x8B},
         8, reinterpret_cast<void*>(&invMat34PtWrapper),
         &g_invMat34Pt, true},
        {"fxpscyloutwardvel", 0x00265420,
         {0x8B, 0x44, 0x24, 0x08, 0xF3, 0x0F, 0x10, 0x08,
          0xF3, 0x0F, 0x10, 0x40, 0x04, 0x0F, 0x28, 0xD1},
         8, reinterpret_cast<void*>(&cylOutwardVelWrapper),
         &g_cylOutwardVel, true},
        {"fxpssplineoutwardvel", 0x002657D0,
         {0x83, 0xEC, 0x0C, 0x0F, 0x57, 0xE4, 0xF3, 0x0F,
          0x10, 0x59, 0x24, 0xF3, 0x0F, 0x10, 0x49, 0x28},
         6, reinterpret_cast<void*>(&splineOutwardVelWrapper),
         &g_splineOutwardVel, true},
        {"fxpsradialoutwardvel", 0x00265B60,
         {0x83, 0xEC, 0x0C, 0x8B, 0x44, 0x24, 0x14, 0xF3,
          0x0F, 0x10, 0x48, 0x04, 0xF3, 0x0F, 0x10, 0x50},
         7, reinterpret_cast<void*>(&radialOutwardVelWrapper),
         &g_radialOutwardVel, true},
        {"fxpseulermat33", 0x00269060,
         {0x83, 0xEC, 0x14, 0x8B, 0x44, 0x24, 0x1C, 0xD9,
          0x00, 0xF3, 0x0F, 0x10, 0x25, 0x08, 0xC0, 0xBD},
         7, reinterpret_cast<void*>(&eulerMat33Wrapper),
         &g_eulerMat33, true},
        {"fxpstransposemat33", 0x00269150,
         {0x8B, 0x4C, 0x24, 0x08, 0x8B, 0x44, 0x24, 0x04,
          0x56, 0x83, 0xC1, 0x18, 0x8D, 0x50, 0x08, 0xBE},
         8, reinterpret_cast<void*>(&transposeMat33Wrapper),
         &g_transposeMat33, true},
    });
}

}  // namespace ra3hook
