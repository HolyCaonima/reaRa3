#include "../Targets.h"

#include <ra3/gameclient/ParticleModules.h>

#include <cstdint>
#include <cstring>

namespace ra3hook {
namespace {

using ra3::gameclient::EmitterModuleRuntimeData;
using ra3::gameclient::GpuDrawModuleView;
using ra3::gameclient::PointVelocityTemplateData;
using ra3::gameclient::VolumeModuleView;

TargetState g_cylindricalVelocityRuntime;
TargetState g_lineVolumeRuntime;
TargetState g_pointVelocityTemplate;
TargetState g_splineVolumeRuntime;
TargetState g_updateLifetime;
TargetState g_updateSize;
TargetState g_updateRotation;
TargetState g_gpuFramesPerRow;
TargetState g_gpuTotalFrames;
TargetState g_gpuSpeedMultiplier;
TargetState g_gpuSingleRow;
TargetState g_gpuSortParticles;
TargetState g_gpuMiscWord;
TargetState g_gpuEnableSoftParticles;
TargetState g_gpuMiscValues;

using RuntimeInitializer =
    void (*)(EmitterModuleRuntimeData&, const std::uint8_t*) noexcept;

EmitterModuleRuntimeData* shadowRuntimeInitializer(
    const char* key, TargetState& state, EmitterModuleRuntimeData* destination,
    const std::uint8_t* source, RuntimeInitializer implementation)
{
    state.calls.fetch_add(1, std::memory_order_relaxed);
    EmitterModuleRuntimeData ours = *destination;
    implementation(ours, source);
    if (state.mode == Mode::replace) {
        *destination = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return destination;
    }
    reinterpret_cast<void(__thiscall*)(
        EmitterModuleRuntimeData*, const std::uint8_t*)>(
            state.original)(destination, source);
    recordOutput(key, state, destination, &ours, sizeof(ours));
    return destination;
}

EmitterModuleRuntimeData* __fastcall cylindricalVelocityRuntime(
    EmitterModuleRuntimeData* destination, void*, const std::uint8_t* source)
{
    return shadowRuntimeInitializer(
        "fxpscylvelocityruntime", g_cylindricalVelocityRuntime,
        destination, source, &ra3::gameclient::initializeCylindricalVelocityRuntime);
}

EmitterModuleRuntimeData* __fastcall lineVolumeRuntime(
    EmitterModuleRuntimeData* destination, void*, const std::uint8_t* source)
{
    return shadowRuntimeInitializer(
        "fxpslinevolumeruntime", g_lineVolumeRuntime,
        destination, source, &ra3::gameclient::initializeLineVolumeRuntime);
}

EmitterModuleRuntimeData* __fastcall splineVolumeRuntime(
    EmitterModuleRuntimeData* destination, void*, const std::uint8_t* source)
{
    return shadowRuntimeInitializer(
        "fxpssplinevolumeruntime", g_splineVolumeRuntime,
        destination, source, &ra3::gameclient::initializeSplineVolumeRuntime);
}

PointVelocityTemplateData* __fastcall pointVelocityTemplate(
    PointVelocityTemplateData* destination, void*, const std::uint8_t* source)
{
    TargetState& state = g_pointVelocityTemplate;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    PointVelocityTemplateData ours{};
    ra3::gameclient::initializePointVelocityTemplate(ours, source);
    if (state.mode == Mode::replace) {
        *destination = ours;
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return destination;
    }
    reinterpret_cast<void(__thiscall*)(
        PointVelocityTemplateData*, const std::uint8_t*)>(
            state.original)(destination, source);
    recordOutput(
        "fxpspointvelocitytemplate", state, destination, &ours, sizeof(ours));
    return destination;
}

using UpdateAccessor = std::uint8_t* (*)(VolumeModuleView&) noexcept;

std::uint8_t* shadowUpdateAccessor(
    const char* key, TargetState& state, VolumeModuleView* module,
    UpdateAccessor implementation)
{
    state.calls.fetch_add(1, std::memory_order_relaxed);
    std::uint8_t* const ours = implementation(*module);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    std::uint8_t* const game =
        reinterpret_cast<std::uint8_t*(__thiscall*)(VolumeModuleView*)>(
            state.original)(module);
    recordScalar(
        key, state, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(game)),
        static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ours)));
    return game;
}

std::uint8_t* __fastcall updateLifetime(VolumeModuleView* module, void*)
{
    return shadowUpdateAccessor(
        "fxpsupdatelifetime", g_updateLifetime, module,
        &ra3::gameclient::defaultUpdateLifetimeData);
}

std::uint8_t* __fastcall updateSize(VolumeModuleView* module, void*)
{
    return shadowUpdateAccessor(
        "fxpsupdatesize", g_updateSize, module,
        &ra3::gameclient::defaultUpdateSizeData);
}

std::uint8_t* __fastcall updateRotation(VolumeModuleView* module, void*)
{
    return shadowUpdateAccessor(
        "fxpsupdaterotation", g_updateRotation, module,
        &ra3::gameclient::defaultUpdateRotationData);
}

using GpuScalarImplementation =
    std::uint32_t (*)(const GpuDrawModuleView&) noexcept;

std::uint32_t shadowGpuScalar(
    const char* key, TargetState& state, const GpuDrawModuleView* module,
    GpuScalarImplementation implementation)
{
    state.calls.fetch_add(1, std::memory_order_relaxed);
    const std::uint32_t ours = implementation(*module);
    if (state.mode == Mode::replace) {
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return ours;
    }
    const std::uint32_t game =
        reinterpret_cast<std::uint32_t(__thiscall*)(const GpuDrawModuleView*)>(
            state.original)(module);
    recordScalar(key, state, game, ours);
    return game;
}

#define RA3_GPU_SCALAR_WRAPPER(wrapper, keyText, stateName, implementation) \
    std::uint32_t __fastcall wrapper(const GpuDrawModuleView* module, void*) \
    { \
        return shadowGpuScalar(keyText, stateName, module, implementation); \
    }

RA3_GPU_SCALAR_WRAPPER(
    gpuFramesPerRow, "fxpsgpuframesperrow", g_gpuFramesPerRow,
    &ra3::gameclient::gpuFramesPerRow)
RA3_GPU_SCALAR_WRAPPER(
    gpuTotalFrames, "fxpsgputotalframes", g_gpuTotalFrames,
    &ra3::gameclient::gpuTotalFrames)
RA3_GPU_SCALAR_WRAPPER(
    gpuSingleRow, "fxpsgpusinglerow", g_gpuSingleRow,
    &ra3::gameclient::gpuSingleRow)
RA3_GPU_SCALAR_WRAPPER(
    gpuSortParticles, "fxpsgpusortparticles", g_gpuSortParticles,
    &ra3::gameclient::gpuSortParticles)
RA3_GPU_SCALAR_WRAPPER(
    gpuMiscWord, "fxpsgpumiscword", g_gpuMiscWord,
    &ra3::gameclient::gpuMiscWord)
RA3_GPU_SCALAR_WRAPPER(
    gpuEnableSoftParticles, "fxpsgpusoftparticles", g_gpuEnableSoftParticles,
    &ra3::gameclient::gpuEnableSoftParticles)

#undef RA3_GPU_SCALAR_WRAPPER

int __cdecl beginGpuSpeed()
{
    TargetState& state = g_gpuSpeedMultiplier;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    return static_cast<int>(state.mode);
}

void* __cdecl gpuSpeedOriginal()
{
    return g_gpuSpeedMultiplier.original;
}

void __cdecl compareGpuSpeed(
    const std::uint8_t* game, const std::uint8_t* ours)
{
    recordOutput(
        "fxpsgpuspeed", g_gpuSpeedMultiplier, game, ours, 10);
}

void __cdecl recordGpuSpeedReplacement()
{
    g_gpuSpeedMultiplier.matches.fetch_add(1, std::memory_order_relaxed);
}

// ra3_1.0.game 0x0066AB70 returns an m32 load in the full x87 ST0 register.
// Keep both shadow results in ten-byte buffers so neither passes through a
// C++ float temporary.
__declspec(naked) float gpuSpeedMultiplier()
{
    __asm {
        sub esp, 24
        mov dword ptr [esp + 20], ecx
        call beginGpuSpeed
        cmp eax, 2
        je replace_path

        call gpuSpeedOriginal
        mov ecx, dword ptr [esp + 20]
        call eax
        fstp tbyte ptr [esp]

        mov ecx, dword ptr [esp + 20]
        mov eax, dword ptr [ecx + 8]
        fld dword ptr [eax + 24h]
        fstp tbyte ptr [esp + 10]

        lea edx, [esp]
        lea eax, [esp + 10]
        push eax
        push edx
        call compareGpuSpeed
        add esp, 8

        fld tbyte ptr [esp]
        add esp, 24
        ret

    replace_path:
        mov ecx, dword ptr [esp + 20]
        mov eax, dword ptr [ecx + 8]
        fld dword ptr [eax + 24h]
        fstp tbyte ptr [esp + 10]
        call recordGpuSpeedReplacement
        fld tbyte ptr [esp + 10]
        add esp, 24
        ret
    }
}

void __fastcall gpuMiscValues(
    const GpuDrawModuleView* module, void*, float* output)
{
    TargetState& state = g_gpuMiscValues;
    state.calls.fetch_add(1, std::memory_order_relaxed);
    float ours[4]{};
    ra3::gameclient::gpuMiscValues(*module, ours);
    if (state.mode == Mode::replace) {
        std::memcpy(output, ours, sizeof(ours));
        state.matches.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    reinterpret_cast<void(__thiscall*)(
        const GpuDrawModuleView*, float*)>(state.original)(module, output);
    recordOutput("fxpsgpumiscvalues", state, output, ours, sizeof(ours));
}

}  // namespace

void appendGameClientTargets(std::vector<Target>& table)
{
    table.insert(table.end(), {
        {"fxpscylvelocityruntime", 0x002620A0,
         {0x8B, 0xC1, 0x8B, 0x4C, 0x24, 0x04, 0xC7, 0x00,
          0x98, 0x2A, 0xBF, 0x00, 0xC7, 0x40, 0x04, 0x05},
         6, reinterpret_cast<void*>(&cylindricalVelocityRuntime),
         &g_cylindricalVelocityRuntime, true},
        {"fxpslinevolumeruntime", 0x00262330,
         {0x8B, 0xC1, 0x8B, 0x4C, 0x24, 0x04, 0xC7, 0x00,
          0x98, 0x2A, 0xBF, 0x00, 0xC7, 0x40, 0x04, 0x01},
         6, reinterpret_cast<void*>(&lineVolumeRuntime), &g_lineVolumeRuntime, true},
        {"fxpspointvelocitytemplate", 0x00262570,
         {0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C, 0x8B, 0xF1,
          0x8D, 0x47, 0x04, 0x50, 0x8D, 0x4E, 0x08, 0xC7},
         6, reinterpret_cast<void*>(&pointVelocityTemplate),
         &g_pointVelocityTemplate, true},
        {"fxpssplinevolumeruntime", 0x002627C0,
         {0x8B, 0xC1, 0x8B, 0x4C, 0x24, 0x04, 0xC7, 0x00,
          0x98, 0x2A, 0xBF, 0x00, 0xC7, 0x40, 0x04, 0x02},
         6, reinterpret_cast<void*>(&splineVolumeRuntime),
         &g_splineVolumeRuntime, true},
        {"fxpsupdatelifetime", 0x0026A710,
         {0x8B, 0x41, 0x08, 0x83, 0xC0, 0x44, 0xC3, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&updateLifetime), &g_updateLifetime, true},
        {"fxpsupdatesize", 0x0026A720,
         {0x8B, 0x41, 0x08, 0x83, 0xC0, 0x50, 0xC3, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&updateSize), &g_updateSize, true},
        {"fxpsupdaterotation", 0x0026A730,
         {0x8B, 0x41, 0x08, 0x83, 0xC0, 0x5C, 0xC3, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&updateRotation), &g_updateRotation, true},
        {"fxpsgpuframesperrow", 0x0026AB50,
         {0x8B, 0x41, 0x08, 0x8B, 0x40, 0x0C, 0xC3, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&gpuFramesPerRow), &g_gpuFramesPerRow, true},
        {"fxpsgputotalframes", 0x0026AB60,
         {0x8B, 0x41, 0x08, 0x8B, 0x40, 0x10, 0xC3, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&gpuTotalFrames), &g_gpuTotalFrames, true},
        {"fxpsgpuspeed", 0x0026AB70,
         {0x8B, 0x41, 0x08, 0xD9, 0x40, 0x24, 0xC3, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&gpuSpeedMultiplier), &g_gpuSpeedMultiplier, true},
        {"fxpsgpusinglerow", 0x0026AB80,
         {0x8B, 0x41, 0x08, 0x8B, 0x40, 0x14, 0xC3, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&gpuSingleRow), &g_gpuSingleRow, true},
        {"fxpsgpusortparticles", 0x0026AB90,
         {0x8B, 0x41, 0x08, 0x8B, 0x40, 0x28, 0xC3, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&gpuSortParticles), &g_gpuSortParticles, true},
        {"fxpsgpumiscword", 0x0026ABA0,
         {0x8B, 0x41, 0x08, 0x8B, 0x40, 0x40, 0xC3, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&gpuMiscWord), &g_gpuMiscWord, true},
        {"fxpsgpusoftparticles", 0x0026ABB0,
         {0x8B, 0x41, 0x08, 0x8A, 0x40, 0x2D, 0xC3, 0xCC,
          0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC},
         6, reinterpret_cast<void*>(&gpuEnableSoftParticles),
         &g_gpuEnableSoftParticles, true},
        {"fxpsgpumiscvalues", 0x0026ABC0,
         {0x8B, 0x49, 0x08, 0x8B, 0x44, 0x24, 0x04, 0xD9,
          0x41, 0x30, 0xD9, 0x18, 0xD9, 0x41, 0x34, 0xD9},
         7, reinterpret_cast<void*>(&gpuMiscValues), &g_gpuMiscValues, true},
    });
}

}  // namespace ra3hook
