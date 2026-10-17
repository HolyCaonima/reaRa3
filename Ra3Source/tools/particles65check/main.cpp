#include <ra3/gameclient/ParticleModules65.h>

#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace ra3;
using namespace ra3::gameclient;

float fromBits(std::uint32_t bits) noexcept {
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool setFpuControlWord(std::uint16_t cw) noexcept {
#if defined(_M_IX86)
    __asm {
        fldcw cw
    }
    return true;
#else
    (void)cw;
    return false;
#endif
}

std::vector<std::uint8_t> decodeHex(const std::string& hex) {
    std::vector<std::uint8_t> out(hex.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<std::uint8_t>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
    }
    return out;
}

template <typename T>
bool decodeStruct(const std::string& hex, T& out) {
    if (hex.size() != sizeof(T) * 2) {
        return false;
    }
    const auto bytes = decodeHex(hex);
    std::memcpy(&out, bytes.data(), sizeof(T));
    return true;
}

std::string encodeBytes(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        out << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    return out.str();
}

template <typename T>
std::string encodeStruct(const T& value) {
    return encodeBytes(&value, sizeof(T));
}

bool decodeState(const std::string& hex, sim::RandomState& state) {
    return decodeStruct(hex, state.words);
}

std::string encodeState(const sim::RandomState& state) {
    return encodeStruct(state.words);
}

} // namespace

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        if (cmd == "quit") {
            return 0;
        }
        if (cmd == "fpcw") {
            std::uint32_t word = 0;
            if (!(in >> word) || !setFpuControlWord(static_cast<std::uint16_t>(word))) {
                std::cout << "error\n";
                continue;
            }
            std::cout << "ok\n";
            continue;
        }
        if (cmd == "alpha_rate") {
            std::string modHex, partHex;
            DefaultAlphaParticleModule mod{};
            FXParticle part{};
            if (!(in >> modHex >> partHex) || !decodeStruct(modHex, mod) || !decodeStruct(partHex, part)) {
                std::cout << "error\n";
                continue;
            }
            mod.particle = &part;
            computeDefaultAlphaRate(mod);
            mod.particle = nullptr;
            std::cout << encodeStruct(mod) << '\n';
            continue;
        }
        if (cmd == "alpha_update") {
            std::string modHex, partHex;
            std::uint32_t shaderType = 0;
            DefaultAlphaParticleModule mod{};
            FXParticle part{};
            if (!(in >> modHex >> partHex >> shaderType) || !decodeStruct(modHex, mod) || !decodeStruct(partHex, part)) {
                std::cout << "error\n";
                continue;
            }
            FXParticleSystemTemplateStub sysTmpl{};
            sysTmpl.shaderType = shaderType;
            FXParticleSystemStub sys{};
            sys.tmpl = &sysTmpl;
            FXParticleSystemRef sysRef{&sys, 1};
            part.systemRef = &sysRef;
            mod.particle = &part;
            updateDefaultAlphaParticle(mod);
            mod.particle = nullptr;
            std::cout << encodeStruct(mod) << '\n';
            continue;
        }
        if (cmd == "alpha_init") {
            std::string tmplHex, partHex, stateHex;
            DefaultAlphaTemplateData tmpl{};
            FXParticle part{};
            sim::RandomState rng{};
            if (!(in >> tmplHex >> partHex >> stateHex) ||
                !decodeStruct(tmplHex, tmpl) ||
                !decodeStruct(partHex, part) ||
                !decodeState(stateHex, rng)) {
                std::cout << "error\n";
                continue;
            }
            DefaultAlphaModuleView alphaMod{0u, nullptr, &tmpl};
            DefaultAlphaParticleModule mod{};
            std::memset(&mod, 0xCC, sizeof(mod));
            initializeDefaultAlphaParticle(mod, &part, alphaMod, rng);
            mod.particle = nullptr;
            std::cout << encodeStruct(mod) << ' ' << encodeState(rng) << '\n';
            continue;
        }
        if (cmd == "color_mod_init") {
            std::string tmplHex;
            std::int32_t refCount = 0;
            DefaultColorModuleTemplate tmpl{};
            if (!(in >> tmplHex >> refCount) || !decodeStruct(tmplHex, tmpl)) {
                std::cout << "error\n";
                continue;
            }
            FXParticleSystemStub sys{};
            sys.refCount = refCount;
            FXParticleSystemStub* sysPtr = &sys;
            DefaultColorModule mod{};
            std::memset(&mod, 0, sizeof(mod));
            initializeDefaultColorModule(mod, &sysPtr, &tmpl);
            mod.system = nullptr;
            mod.templatePtr = nullptr;
            std::cout << encodeStruct(mod) << ' ' << sys.refCount << '\n';
            continue;
        }
        if (cmd == "color_scale_range") {
            std::string modHex;
            DefaultColorModule mod{};
            if (!(in >> modHex) || !decodeStruct(modHex, mod)) {
                std::cout << "error\n";
                continue;
            }
            float outRange[2] = {0.0f, 0.0f};
            getDefaultColorScaleRange(mod, outRange);
            std::cout << encodeBytes(outRange, sizeof(outRange)) << '\n';
            continue;
        }
        if (cmd == "color_tint") {
            std::string modHex;
            std::uint32_t packedRgb = 0;
            DefaultColorModule mod{};
            if (!(in >> modHex >> packedRgb) || !decodeStruct(modHex, mod)) {
                std::cout << "error\n";
                continue;
            }
            applyDefaultColorTint(mod, packedRgb);
            std::cout << encodeStruct(mod) << '\n';
            continue;
        }
        if (cmd == "color_rate") {
            std::string modHex, partHex;
            DefaultColorParticleModule mod{};
            FXParticle part{};
            if (!(in >> modHex >> partHex) || !decodeStruct(modHex, mod) || !decodeStruct(partHex, part)) {
                std::cout << "error\n";
                continue;
            }
            mod.particle = &part;
            computeDefaultColorRate(mod);
            mod.particle = nullptr;
            std::cout << encodeStruct(mod) << '\n';
            continue;
        }
        if (cmd == "color_update") {
            std::string modHex, partHex;
            DefaultColorParticleModule mod{};
            FXParticle part{};
            if (!(in >> modHex >> partHex) || !decodeStruct(modHex, mod) || !decodeStruct(partHex, part)) {
                std::cout << "error\n";
                continue;
            }
            mod.particle = &part;
            updateDefaultColorParticle(mod);
            mod.particle = nullptr;
            std::cout << encodeStruct(mod) << '\n';
            continue;
        }
        if (cmd == "color_part_init") {
            std::string colorModHex, partHex, stateHex;
            DefaultColorModule colorMod{};
            FXParticle part{};
            sim::RandomState rng{};
            if (!(in >> colorModHex >> partHex >> stateHex) ||
                !decodeStruct(colorModHex, colorMod) ||
                !decodeStruct(partHex, part) ||
                !decodeState(stateHex, rng)) {
                std::cout << "error\n";
                continue;
            }
            DefaultColorParticleModule mod{};
            std::memset(&mod, 0, sizeof(mod));
            initializeDefaultColorParticle(mod, &part, colorMod, rng);
            mod.particle = nullptr;
            std::cout << encodeStruct(mod) << ' ' << encodeState(rng) << '\n';
            continue;
        }
        if (cmd == "wind_tmpl_init") {
            std::string assetHex;
            WindModuleAssetData asset{};
            if (!(in >> assetHex) || !decodeStruct(assetHex, asset)) {
                std::cout << "error\n";
                continue;
            }
            WindModuleTemplate tmpl{};
            initializeWindModuleTemplateFromAsset(tmpl, asset);
            std::cout << encodeStruct(tmpl) << '\n';
            continue;
        }
        if (cmd == "wind_tmpl_copy") {
            std::string srcHex;
            WindModuleTemplate src{};
            if (!(in >> srcHex) || !decodeStruct(srcHex, src)) {
                std::cout << "error\n";
                continue;
            }
            WindModuleTemplate dst{};
            copyWindModuleTemplate(dst, src);
            std::cout << encodeStruct(dst) << '\n';
            continue;
        }
        if (cmd == "wind_mod_init") {
            std::string tmplHex, stateHex;
            std::int32_t refCount = 0;
            WindModuleTemplate tmpl{};
            sim::RandomState rng{};
            if (!(in >> tmplHex >> refCount >> stateHex) ||
                !decodeStruct(tmplHex, tmpl) ||
                !decodeState(stateHex, rng)) {
                std::cout << "error\n";
                continue;
            }
            FXParticleSystemStub sys{};
            sys.refCount = refCount;
            FXParticleSystemStub* sysPtr = &sys;
            WindModule mod{};
            std::memset(&mod, 0, sizeof(mod));
            initializeWindModule(mod, &sysPtr, &tmpl, rng);
            mod.system = nullptr;
            mod.templatePtr = nullptr;
            std::cout << encodeStruct(mod) << ' ' << sys.refCount << ' ' << encodeState(rng) << '\n';
            continue;
        }
        if (cmd == "wind_mod_update") {
            std::string modHex, tmplHex, stateHex;
            WindModule mod{};
            WindModuleTemplate tmpl{};
            sim::RandomState rng{};
            if (!(in >> modHex >> tmplHex >> stateHex) ||
                !decodeStruct(modHex, mod) ||
                !decodeStruct(tmplHex, tmpl) ||
                !decodeState(stateHex, rng)) {
                std::cout << "error\n";
                continue;
            }
            mod.templatePtr = &tmpl;
            updateWindModuleMotion(mod, rng);
            mod.templatePtr = nullptr;
            std::cout << encodeStruct(mod) << ' ' << encodeState(rng) << '\n';
            continue;
        }
        if (cmd == "particle_size") {
            int kind = 0;
            std::uint32_t sizeBits = 0;
            if (!(in >> kind >> sizeBits)) {
                std::cout << "error\n";
                continue;
            }
            FXParticle part{};
            DefaultUpdateParticleModule defMod{};
            RenderObjectParticleUpdateModule roMod{};
            if (kind == 1) {
                defMod.vtable = 0x00BF3F9Cu;
                defMod.size = fromBits(sizeBits);
                part.updateParticleModule = &defMod;
            } else if (kind == 2) {
                roMod.vtable = 0x00BF4418u;
                roMod.scale.x = fromBits(sizeBits);
                part.updateParticleModule = &roMod;
            } else {
                part.updateParticleModule = nullptr;
            }
            std::uint8_t st0[10]{};
            getParticleSizeExtended(part, st0);
            std::cout << encodeBytes(st0, 10) << '\n';
            continue;
        }
        if (cmd == "wind_part_update") {
            std::string wpHex, partHex, wmHex, wtHex;
            std::uint32_t sx = 0, sy = 0, sz = 0, sizeBits = 0;
            int sizeKind = 0;
            WindParticleModule wp{};
            FXParticle part{};
            WindModule wm{};
            WindModuleTemplate wt{};
            if (!(in >> wpHex >> partHex >> wmHex >> wtHex >> sx >> sy >> sz >> sizeKind >> sizeBits) ||
                !decodeStruct(wpHex, wp) ||
                !decodeStruct(partHex, part) ||
                !decodeStruct(wmHex, wm) ||
                !decodeStruct(wtHex, wt)) {
                std::cout << "error\n";
                continue;
            }
            wm.templatePtr = &wt;
            FXParticleSystemStub sys{};
            sys.windModule = &wm;
            sys.worldTransform.m[3] = fromBits(sx);
            sys.worldTransform.m[7] = fromBits(sy);
            sys.worldTransform.m[11] = fromBits(sz);
            FXParticleSystemRef sysRef{&sys, 1};
            part.systemRef = &sysRef;

            DefaultUpdateParticleModule defMod{};
            RenderObjectParticleUpdateModule roMod{};
            if (sizeKind == 1) {
                defMod.vtable = 0x00BF3F9Cu;
                defMod.size = fromBits(sizeBits);
                part.updateParticleModule = &defMod;
            } else if (sizeKind == 2) {
                roMod.vtable = 0x00BF4418u;
                roMod.scale.x = fromBits(sizeBits);
                part.updateParticleModule = &roMod;
            } else {
                part.updateParticleModule = nullptr;
            }

            wp.particle = &part;
            updateWindParticle(wp);
            std::cout << encodeStruct(part.position) << '\n';
            continue;
        }
        if (cmd == "upd_base_copy") {
            std::string srcHex;
            UpdateModuleTemplate src{};
            if (!(in >> srcHex) || !decodeStruct(srcHex, src)) {
                std::cout << "error\n";
                continue;
            }
            UpdateModuleTemplate dst{};
            copyUpdateModuleTemplateBase(dst, src);
            std::cout << encodeStruct(dst) << '\n';
            continue;
        }
        if (cmd == "def_upd_tmpl_init") {
            std::string assetHex;
            DefaultUpdateModuleAssetData asset{};
            if (!(in >> assetHex) || !decodeStruct(assetHex, asset)) {
                std::cout << "error\n";
                continue;
            }
            DefaultUpdateModuleTemplate tmpl{};
            initializeDefaultUpdateModuleTemplateFromAsset(tmpl, asset);
            std::cout << encodeStruct(tmpl) << '\n';
            continue;
        }
        if (cmd == "def_upd_tmpl_copy") {
            std::string srcHex;
            DefaultUpdateModuleTemplate src{};
            if (!(in >> srcHex) || !decodeStruct(srcHex, src)) {
                std::cout << "error\n";
                continue;
            }
            DefaultUpdateModuleTemplate dst{};
            copyDefaultUpdateModuleTemplate(dst, src);
            std::cout << encodeStruct(dst) << '\n';
            continue;
        }
        if (cmd == "def_upd_part_init") {
            std::string tmplHex, sysRvHex, stateHex;
            std::uint32_t sysMulBits = 0, sysBiasBits = 0, globalMulBits = 0;
            DefaultUpdateModuleTemplate tmpl{};
            sim::RandomVariable sysRv{};
            sim::RandomState rng{};
            if (!(in >> tmplHex >> sysMulBits >> sysBiasBits >> sysRvHex >> globalMulBits >> stateHex) ||
                !decodeStruct(tmplHex, tmpl) ||
                !decodeStruct(sysRvHex, sysRv) ||
                !decodeState(stateHex, rng)) {
                std::cout << "error\n";
                continue;
            }
            FXParticleSystemTemplateStub sysTmpl{};
            sysTmpl.systemSizeScaleRand = sysRv;
            FXParticleSystemStub sys{};
            sys.tmpl = &sysTmpl;
            sys.sizeMultiplier = fromBits(sysMulBits);
            sys.sizeBias = fromBits(sysBiasBits);
            FXParticleSystemRef sysRef{&sys, 1};
            DefaultUpdateModuleView updMod{0u, &sysRef, &tmpl};
            FXParticle part{};
            DefaultUpdateParticleModule mod{};
            std::memset(&mod, 0, sizeof(mod));
            initializeDefaultUpdateParticle(mod, &part, updMod, fromBits(globalMulBits), rng);
            mod.particle = nullptr;
            std::cout << encodeStruct(mod) << ' ' << encodeState(rng) << '\n';
            continue;
        }
        if (cmd == "def_upd_part_update") {
            std::string modHex;
            DefaultUpdateParticleModule mod{};
            if (!(in >> modHex) || !decodeStruct(modHex, mod)) {
                std::cout << "error\n";
                continue;
            }
            updateDefaultUpdateParticle(mod);
            std::cout << encodeStruct(mod) << '\n';
            continue;
        }
        if (cmd == "ro_upd_tmpl_init") {
            std::string assetHex;
            RenderObjectUpdateModuleAssetData asset{};
            if (!(in >> assetHex) || !decodeStruct(assetHex, asset)) {
                std::cout << "error\n";
                continue;
            }
            RenderObjectUpdateModuleTemplate tmpl{};
            initializeRenderObjectUpdateModuleTemplateFromAsset(tmpl, asset);
            std::cout << encodeStruct(tmpl) << '\n';
            continue;
        }
        if (cmd == "ro_upd_tmpl_copy") {
            std::string srcHex;
            RenderObjectUpdateModuleTemplate src{};
            if (!(in >> srcHex) || !decodeStruct(srcHex, src)) {
                std::cout << "error\n";
                continue;
            }
            RenderObjectUpdateModuleTemplate dst{};
            copyRenderObjectUpdateModuleTemplate(dst, src);
            std::cout << encodeStruct(dst) << '\n';
            continue;
        }
        if (cmd == "ro_upd_part_init") {
            std::string tmplHex, sysRvHex, stateHex;
            std::uint32_t sysYMulBits = 0, sysBiasBits = 0;
            RenderObjectUpdateModuleTemplate tmpl{};
            sim::RandomVariable sysRv{};
            sim::RandomState rng{};
            if (!(in >> tmplHex >> sysYMulBits >> sysBiasBits >> sysRvHex >> stateHex) ||
                !decodeStruct(tmplHex, tmpl) ||
                !decodeStruct(sysRvHex, sysRv) ||
                !decodeState(stateHex, rng)) {
                std::cout << "error\n";
                continue;
            }
            FXParticleSystemTemplateStub sysTmpl{};
            sysTmpl.systemSizeScaleRand = sysRv;
            FXParticleSystemStub sys{};
            sys.tmpl = &sysTmpl;
            sys.sizeBias = fromBits(sysBiasBits);
            sys.renderObjectScaleYMultiplier = fromBits(sysYMulBits);
            FXParticleSystemRef sysRef{&sys, 1};
            RenderObjectUpdateModuleView updMod{0u, &sysRef, &tmpl};
            FXParticle part{};
            RenderObjectParticleUpdateModule mod{};
            std::memset(&mod, 0, sizeof(mod));
            initializeRenderObjectUpdateParticle(mod, &part, updMod, rng);
            mod.particle = nullptr;
            std::cout << encodeStruct(mod) << ' ' << encodeState(rng) << '\n';
            continue;
        }
        if (cmd == "ro_upd_part_update") {
            std::string modHex;
            RenderObjectParticleUpdateModule mod{};
            if (!(in >> modHex) || !decodeStruct(modHex, mod)) {
                std::cout << "error\n";
                continue;
            }
            updateRenderObjectUpdateParticle(mod);
            std::cout << encodeStruct(mod) << '\n';
            continue;
        }
        if (cmd == "swarm_update") {
            std::string modHex, partHex;
            std::uint32_t sxBits = 0, syBits = 0, szBits = 0;
            SwarmPhysicsParticleModule mod{};
            FXParticle part{};
            if (!(in >> modHex >> partHex >> sxBits >> syBits >> szBits) ||
                !decodeStruct(modHex, mod) ||
                !decodeStruct(partHex, part)) {
                std::cout << "error\n";
                continue;
            }
            FXParticleSystemStub sys{};
            sys.worldTransform.m[3] = fromBits(sxBits);
            sys.worldTransform.m[7] = fromBits(syBits);
            sys.worldTransform.m[11] = fromBits(szBits);
            FXParticleSystemRef sysRef{&sys, 1};
            part.systemRef = &sysRef;
            mod.particle = &part;
            updateSwarmPhysicsParticle(mod);
            std::cout << encodeStruct(part.velocity) << encodeStruct(part.position) << '\n';
            continue;
        }
        if (cmd == "wave_tmpl_init") {
            std::string assetHex;
            WavefrontVelocityAssetData asset{};
            if (!(in >> assetHex) || !decodeStruct(assetHex, asset)) {
                std::cout << "error\n";
                continue;
            }
            WavefrontEmissionVelocityModuleTemplate tmpl{};
            initializeWavefrontVelocityTemplateFromAsset(tmpl, asset);
            std::cout << encodeStruct(tmpl) << '\n';
            continue;
        }
        if (cmd == "wave_tmpl_copy") {
            std::string srcHex;
            WavefrontEmissionVelocityModuleTemplate src{};
            if (!(in >> srcHex) || !decodeStruct(srcHex, src)) {
                std::cout << "error\n";
                continue;
            }
            WavefrontEmissionVelocityModuleTemplate dst{};
            copyWavefrontVelocityTemplate(dst, src);
            std::cout << encodeStruct(dst) << '\n';
            continue;
        }
        if (cmd == "wave_sample") {
            std::string tmplHex, stateHex;
            WavefrontEmissionVelocityModuleTemplate tmpl{};
            sim::RandomState rng{};
            if (!(in >> tmplHex >> stateHex) || !decodeStruct(tmplHex, tmpl) || !decodeState(stateHex, rng)) {
                std::cout << "error\n";
                continue;
            }
            WavefrontEmissionVelocityModule mod{0u, nullptr, &tmpl};
            float out[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            sampleWavefrontVelocityDimensions(mod, out[0], out[1], out[2], out[3], rng);
            std::cout << encodeBytes(out, sizeof(out)) << ' ' << encodeState(rng) << '\n';
            continue;
        }
        if (cmd == "tfire_tmpl_init") {
            std::string assetHex;
            TerrainFireAssetData asset{};
            if (!(in >> assetHex) || !decodeStruct(assetHex, asset)) {
                std::cout << "error\n";
                continue;
            }
            TerrainFireEmissionModuleTemplate tmpl{};
            initializeTerrainFireTemplateFromAsset(tmpl, asset);
            std::cout << encodeStruct(tmpl) << '\n';
            continue;
        }
        if (cmd == "tfire_tmpl_copy") {
            std::string srcHex;
            TerrainFireEmissionModuleTemplate src{};
            if (!(in >> srcHex) || !decodeStruct(srcHex, src)) {
                std::cout << "error\n";
                continue;
            }
            TerrainFireEmissionModuleTemplate dst{};
            copyTerrainFireTemplate(dst, src);
            std::cout << encodeStruct(dst) << '\n';
            continue;
        }
        if (cmd == "tfire_sample") {
            std::string tmplHex, stateHex;
            TerrainFireEmissionModuleTemplate tmpl{};
            sim::RandomState rng{};
            if (!(in >> tmplHex >> stateHex) || !decodeStruct(tmplHex, tmpl) || !decodeState(stateHex, rng)) {
                std::cout << "error\n";
                continue;
            }
            TerrainFireEmissionModule mod{0x00BF4BC0u, nullptr, &tmpl};
            Vector3 out{};
            sampleTerrainFirePoint(mod, out, rng);
            std::cout << encodeStruct(out) << ' ' << encodeState(rng) << '\n';
            continue;
        }
        if (cmd == "ortho_tmpl_copy") {
            std::string srcHex;
            PointVelocityTemplateData src{};
            if (!(in >> srcHex) || !decodeStruct(srcHex, src)) {
                std::cout << "error\n";
                continue;
            }
            PointVelocityTemplateData dst{};
            copyOrthoVelocityTemplate(dst, src);
            std::cout << encodeStruct(dst) << '\n';
            continue;
        }
        if (cmd == "outward_vel") {
            std::string velTmplHex, posHex, volDataHex, stateHex;
            std::uint32_t volVt = 0;
            OutwardEmissionVelocityModuleTemplate velTmpl{};
            Vector3 pos{};
            sim::RandomState rng{};
            if (!(in >> velTmplHex >> posHex >> volVt >> volDataHex >> stateHex) ||
                !decodeStruct(velTmplHex, velTmpl) ||
                !decodeStruct(posHex, pos) ||
                !decodeState(stateHex, rng)) {
                std::cout << "error\n";
                continue;
            }
            OutwardEmissionVelocityModule velMod{0x00BF4AB0u, nullptr, &velTmpl};
            Vector3 outVel{};
            if (volVt == 0x00BF49DCu) {
                CylinderEmissionVolumeData cylTmpl{};
                if (!decodeStruct(volDataHex, cylTmpl.offset)) {
                    std::cout << "error\n";
                    continue;
                }
                CylinderVolumeModuleView cylMod{{volVt, 0u, &cylTmpl}, 1.0f};
                computeOutwardEmissionVelocity(
                    velMod, outVel, pos, reinterpret_cast<const VolumeModuleView&>(cylMod), rng);
            } else if (volVt == 0x00BF4A2Cu) {
                Vector3 pts[2]{};
                if (!decodeStruct(volDataHex, pts)) {
                    std::cout << "error\n";
                    continue;
                }
                LineEmissionVolumeData lineTmpl{};
                lineTmpl.start = pts[0];
                lineTmpl.end = pts[1];
                VolumeModuleView lineMod{volVt, 0u, &lineTmpl};
                computeOutwardEmissionVelocity(velMod, outVel, pos, lineMod, rng);
            } else if (volVt == 0x00BF4CC4u) {
                SplineEmissionVolumeModule splineMod{};
                if (!decodeStruct(volDataHex, splineMod.tangent)) {
                    std::cout << "error\n";
                    continue;
                }
                splineMod.vtable = volVt;
                computeOutwardEmissionVelocity(
                    velMod, outVel, pos, reinterpret_cast<const VolumeModuleView&>(splineMod), rng);
            } else {
                VolumeModuleView radMod{volVt, 0u, nullptr};
                computeOutwardEmissionVelocity(velMod, outVel, pos, radMod, rng);
            }
            std::cout << encodeStruct(outVel) << ' ' << encodeState(rng) << '\n';
            continue;
        }
        if (cmd == "cyl_step_radius") {
            std::uint32_t rateBits = 0, offsetBits = 0;
            if (!(in >> rateBits >> offsetBits)) {
                std::cout << "error\n";
                continue;
            }
            CylinderEmissionVolumeData tmpl{};
            tmpl.radiusRate = fromBits(rateBits);
            CylinderVolumeModuleView mod{{0x00BF49DCu, 0u, &tmpl}, fromBits(offsetBits)};
            stepCylinderVolumeRadiusOffset(mod);
            std::cout << encodeStruct(mod.currentRadius) << '\n';
            continue;
        }
        if (cmd == "cyl_outward") {
            std::string normHex, posHex;
            std::uint32_t radBits = 0, normBits = 0;
            Vector3 normal{}, pos{};
            if (!(in >> normHex >> posHex >> radBits >> normBits) ||
                !decodeStruct(normHex, normal) ||
                !decodeStruct(posHex, pos)) {
                std::cout << "error\n";
                continue;
            }
            CylinderEmissionVolumeData tmpl{};
            tmpl.offset = normal;
            CylinderVolumeModuleView mod{{0x00BF49DCu, 0u, &tmpl}, 1.0f};
            Vector3 outVel{};
            computeCylinderOutwardVelocity(mod, outVel, pos, fromBits(radBits), fromBits(normBits));
            std::cout << encodeStruct(outVel) << '\n';
            continue;
        }
        if (cmd == "line_outward") {
            std::string ptsHex, posHex;
            std::uint32_t radBits = 0, normBits = 0;
            Vector3 pts[2]{}, pos{};
            if (!(in >> ptsHex >> posHex >> radBits >> normBits) ||
                !decodeStruct(ptsHex, pts) ||
                !decodeStruct(posHex, pos)) {
                std::cout << "error\n";
                continue;
            }
            LineEmissionVolumeData tmpl{};
            tmpl.start = pts[0];
            tmpl.end = pts[1];
            VolumeModuleView mod{0x00BF4A2Cu, 0u, &tmpl};
            Vector3 outVel{};
            computeLineOutwardVelocity(mod, outVel, pos, fromBits(radBits), fromBits(normBits));
            std::cout << encodeStruct(outVel) << '\n';
            continue;
        }
        if (cmd == "spline_outward") {
            std::string tanHex, posHex;
            std::uint32_t radBits = 0, normBits = 0;
            Vector3 tan{}, pos{};
            if (!(in >> tanHex >> posHex >> radBits >> normBits) ||
                !decodeStruct(tanHex, tan) ||
                !decodeStruct(posHex, pos)) {
                std::cout << "error\n";
                continue;
            }
            SplineEmissionVolumeModule mod{};
            mod.vtable = 0x00BF4CC4u;
            mod.tangent = tan;
            Vector3 outVel{};
            computeSplineOutwardVelocity(mod, outVel, pos, fromBits(radBits), fromBits(normBits));
            std::cout << encodeStruct(outVel) << '\n';
            continue;
        }
        if (cmd == "radial_outward") {
            std::string posHex;
            std::uint32_t radBits = 0, normBits = 0;
            Vector3 pos{};
            if (!(in >> posHex >> radBits >> normBits) || !decodeStruct(posHex, pos)) {
                std::cout << "error\n";
                continue;
            }
            VolumeModuleView mod{0x00BF4954u, 0u, nullptr};
            Vector3 outVel{};
            computeRadialOutwardVelocity(mod, outVel, pos, fromBits(radBits), fromBits(normBits));
            std::cout << encodeStruct(outVel) << '\n';
            continue;
        }
        if (cmd == "lgt_tmpl_init") {
            std::string hdrHex, pairsHex;
            float hdr[7]{};
            LightningDrawAssetPair pairs[2]{};
            if (!(in >> hdrHex >> pairsHex) ||
                !decodeStruct(hdrHex, hdr) ||
                !decodeStruct(pairsHex, pairs)) {
                std::cout << "error\n";
                continue;
            }
            LightningDrawAssetData asset{};
            std::memcpy(&asset, hdr, sizeof(hdr));
            asset.pairsPtr = pairs;
            LightningDrawModuleTemplate tmpl{};
            initializeLightningDrawTemplateFromAsset(tmpl, asset);
            std::cout << encodeStruct(tmpl) << '\n';
            continue;
        }
        if (cmd == "lgt_tmpl_copy") {
            std::string srcHex;
            LightningDrawModuleTemplate src{};
            if (!(in >> srcHex) || !decodeStruct(srcHex, src)) {
                std::cout << "error\n";
                continue;
            }
            LightningDrawModuleTemplate dst{};
            copyLightningDrawTemplate(dst, src);
            std::cout << encodeStruct(dst) << '\n';
            continue;
        }
        if (cmd == "lgt_tmpl_default") {
            std::string garHex;
            LightningDrawModuleTemplate tmpl{};
            if (!(in >> garHex) || !decodeStruct(garHex, tmpl)) {
                std::cout << "error\n";
                continue;
            }
            initializeDefaultLightningDrawTemplate(tmpl);
            std::cout << encodeStruct(tmpl) << '\n';
            continue;
        }
        if (cmd == "lgt_mod_init") {
            std::string tmplHex, stateHex;
            std::int32_t refCount = 0;
            std::uint32_t shaderType = 0;
            LightningDrawModuleTemplate tmpl{};
            sim::RandomState rng{};
            if (!(in >> tmplHex >> refCount >> shaderType >> stateHex) ||
                !decodeStruct(tmplHex, tmpl) ||
                !decodeState(stateHex, rng)) {
                std::cout << "error\n";
                continue;
            }
            FXParticleSystemTemplateStub sysTmpl{};
            sysTmpl.shaderType = shaderType;
            FXParticleSystemStub realSys{};
            realSys.tmpl = &sysTmpl;
            FXParticleSystemStub sysRef{};
            sysRef.vtable = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&realSys));
            sysRef.refCount = refCount;
            FXParticleSystemStub* sysPtr = &sysRef;
            LightningDrawModule mod{};
            initializeLightningDrawModule(mod, &sysPtr, &tmpl, rng);
            mod.system = nullptr;
            mod.templatePtr = nullptr;
            std::cout << encodeStruct(mod) << ' ' << sysRef.refCount << ' ' << encodeState(rng) << '\n';
            continue;
        }
        if (cmd == "lgt_pack_verts") {
            std::int32_t segCount = 0;
            std::string partHex;
            FXParticle part{};
            if (!(in >> segCount >> partHex) || !decodeStruct(partHex, part) || segCount < 0 || segCount > 16) {
                std::cout << "error\n";
                continue;
            }
            LightningVertexPackerModule mod{};
            mod.segmentCount = segCount;
            std::vector<LightningSwarmVertex> verts(static_cast<std::size_t>(segCount > 0 ? segCount : 1));
            std::memset(verts.data(), 0, verts.size() * sizeof(LightningSwarmVertex));
            packLightningParticleVertices(mod, part, verts.data());
            std::cout << encodeBytes(verts.data(), verts.size() * sizeof(LightningSwarmVertex)) << '\n';
            continue;
        }
        if (cmd == "ro_draw_tmpl_copy") {
            std::string srcHex;
            RenderObjectDrawModuleTemplate src{};
            if (!(in >> srcHex) || !decodeStruct(srcHex, src)) {
                std::cout << "error\n";
                continue;
            }
            RenderObjectDrawModuleTemplate dst{};
            copyRenderObjectDrawTemplate(dst, src);
            std::cout << encodeStruct(dst) << '\n';
            continue;
        }
        if (cmd == "life_evt_init") {
            std::string rvHex, stateHex;
            std::int32_t refCount = 0;
            sim::RandomVariable rv{};
            sim::RandomState rng{};
            if (!(in >> rvHex >> refCount >> stateHex) ||
                !decodeStruct(rvHex, rv) ||
                !decodeState(stateHex, rng)) {
                std::cout << "error\n";
                continue;
            }
            LifeEventModuleTemplate tmpl{};
            tmpl.eventTimeRand = rv;
            FXParticleSystemStub sys{};
            sys.refCount = refCount;
            FXParticleSystemStub* sysPtr = &sys;
            LifeEventModule mod{};
            initializeLifeEventModule(mod, &sysPtr, &tmpl, rng);
            mod.system = nullptr;
            mod.templatePtr = nullptr;
            std::cout << encodeStruct(mod) << ' ' << sys.refCount << ' ' << encodeState(rng) << '\n';
            continue;
        }
        if (cmd == "part_base_init") {
            std::string spawnHex;
            std::int32_t refCount = 0;
            std::uint32_t activeCount = 0;
            FXParticleSpawnInfo spawn{};
            if (!(in >> spawnHex >> refCount >> activeCount) || !decodeStruct(spawnHex, spawn)) {
                std::cout << "error\n";
                continue;
            }
            FXParticleSystemRef sysRef{nullptr, refCount};
            FXParticleSystemRef* sysRefPtr = &sysRef;
            FXParticle part{};
            std::memset(&part, 0, sizeof(part));
            initializeParticleBase(part, &sysRefPtr, spawn, activeCount);
            part.systemRef = nullptr;
            std::cout << encodeStruct(part) << ' ' << sysRef.refCount << ' ' << activeCount << '\n';
            continue;
        }
        if (cmd == "part_full_init") {
            std::string spawnHex, stateHex;
            std::int32_t refCount = 0;
            std::uint32_t activeCount = 0, clientFrame = 0, frameDurBits = 0;
            FXParticleSpawnInfo spawn{};
            sim::RandomState rng{};
            if (!(in >> spawnHex >> refCount >> activeCount >> clientFrame >> frameDurBits >> stateHex) ||
                !decodeStruct(spawnHex, spawn) ||
                !decodeState(stateHex, rng)) {
                std::cout << "error\n";
                continue;
            }
            FXParticleSystemRef sysRef{nullptr, refCount};
            FXParticleSystemRef* sysRefPtr = &sysRef;
            FXParticle part{};
            std::memset(&part, 0xCC, sizeof(part));
            initializeParticle(part, &sysRefPtr, spawn, activeCount, clientFrame, fromBits(frameDurBits), rng);
            part.systemRef = nullptr;
            std::cout << encodeStruct(part) << ' ' << sysRef.refCount << ' ' << activeCount << ' ' << encodeState(rng) << '\n';
            continue;
        }
        if (cmd == "hermite") {
            std::uint32_t tBits = 0;
            std::string vecsHex;
            Vector3 vecs[4]{};
            if (!(in >> tBits >> vecsHex) || !decodeStruct(vecsHex, vecs)) {
                std::cout << "error\n";
                continue;
            }
            Vector3 out{};
            evaluateCubicHermiteSpline(out, fromBits(tBits), vecs[0], vecs[1], vecs[2], vecs[3]);
            std::cout << encodeStruct(out) << '\n';
            continue;
        }
        if (cmd == "inv_mat34") {
            std::string matHex, ptHex;
            Matrix34 mat{};
            Vector3 pt{};
            if (!(in >> matHex >> ptHex) || !decodeStruct(matHex, mat) || !decodeStruct(ptHex, pt)) {
                std::cout << "error\n";
                continue;
            }
            Vector3 out{};
            inverseTransformPointByMatrix34(mat, pt, out);
            std::cout << encodeStruct(out) << '\n';
            continue;
        }
        if (cmd == "mul_mat34") {
            std::string lhsHex, rhsHex;
            Matrix34 lhs{}, rhs{};
            if (!(in >> lhsHex >> rhsHex) || !decodeStruct(lhsHex, lhs) || !decodeStruct(rhsHex, rhs)) {
                std::cout << "error\n";
                continue;
            }
            Matrix34 out{};
            multiplyMatrix34(out, lhs, rhs);
            std::cout << encodeStruct(out) << '\n';
            continue;
        }
        if (cmd == "ext_rot33") {
            std::string matHex;
            Matrix34 mat{};
            if (!(in >> matHex) || !decodeStruct(matHex, mat)) {
                std::cout << "error\n";
                continue;
            }
            Matrix33 out{};
            extractTransposeRotation33(out, mat);
            std::cout << encodeStruct(out) << '\n';
            continue;
        }
        if (cmd == "euler_mat33") {
            std::string eulerHex;
            Vector3 euler{};
            if (!(in >> eulerHex) || !decodeStruct(eulerHex, euler)) {
                std::cout << "error\n";
                continue;
            }
            Matrix33 out{};
            buildEulerXYZMatrix33(out, euler);
            std::cout << encodeStruct(out) << '\n';
            continue;
        }
        if (cmd == "trans_mat33") {
            std::string inHex;
            Matrix33 inMat{};
            if (!(in >> inHex) || !decodeStruct(inHex, inMat)) {
                std::cout << "error\n";
                continue;
            }
            Matrix33 out{};
            transposeMatrix33(out, inMat);
            std::cout << encodeStruct(out) << '\n';
            continue;
        }
        std::cout << "error\n";
    }
    return 0;
}
