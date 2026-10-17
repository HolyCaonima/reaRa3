#include <ra3/gameclient/ParticleModules66.h>

#include <cstddef>
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

constexpr int kMaxDraws = 16;

struct DrawRec {
    std::uint32_t mode;
    float ax, ay, az;
    std::uint32_t b0, b1, b2;
    std::uint32_t color, a3, a4;
};

static_assert(sizeof(DrawRec) == 40, "draw record layout");

DrawRec g_draws[kMaxDraws];
int g_drawCount = 0;
void* g_drawerVtable[32] = {};
std::uint8_t g_drawerObject[8] = {};
Fx66DebugRoot g_root{};
std::uint32_t g_freedTag = 0;
bool g_allocFail = false;
void* g_memberObject = nullptr;
std::uint8_t g_memberByte = 0;

std::uint32_t bitsOf(float value) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float floatOf(std::uint32_t bits) noexcept {
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool setFpuControlWord(std::uint16_t cw) noexcept {
    __asm {
        fldcw cw
    }
    return true;
}

std::string hexBytes(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < size; ++i) {
        out << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    return out.str();
}

std::string hexU32(std::uint32_t value) {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(8) << value;
    return out.str();
}

void onLine(void*, const float* a, const float* b, std::uint32_t color, std::uint32_t a3, std::uint32_t a4) {
    if (g_drawCount >= kMaxDraws) {
        return;
    }
    auto& rec = g_draws[g_drawCount++];
    rec.mode = 0;
    rec.ax = a[0];
    rec.ay = a[1];
    rec.az = a[2];
    std::memcpy(&rec.b0, b, 12);
    rec.color = color;
    rec.a3 = a3;
    rec.a4 = a4;
}

void onCircle(void*, const float* point, std::uint32_t radius, std::uint32_t color, std::uint32_t a3, std::uint32_t a4) {
    if (g_drawCount >= kMaxDraws) {
        return;
    }
    auto& rec = g_draws[g_drawCount++];
    rec.mode = 1;
    rec.ax = point[0];
    rec.ay = point[1];
    rec.az = point[2];
    rec.b0 = radius;
    rec.b1 = 0;
    rec.b2 = 0;
    rec.color = color;
    rec.a3 = a3;
    rec.a4 = a4;
}

void onMember(void* self) {
    g_memberByte = 0x5A;
    g_memberObject = self;
}

void onPrepare(void* module) {
    static_cast<std::uint8_t*>(module)[0x19] = 0x5A;
}

void onSubmit(void*, void*, void*, void* mesh, void*) {
    static_cast<std::uint8_t*>(mesh)[0x3C] = 0xA5;
}

void* __cdecl testAlloc(std::uint32_t size) {
    if (g_allocFail) {
        return nullptr;
    }
    auto* block = new std::uint8_t[size];
    std::memset(block, 0xCC, size);
    return block;
}

struct NameObject {
    void* vtable;
    volatile long refs;
    std::uint32_t tag;
    std::uint8_t gap[0x18];
    std::uint8_t mark;
};

struct HolderObject {
    void* vtable;
    volatile long refs;
    std::uint8_t pad[0x14];
    NameObject* name;
    std::uint32_t tag;
    std::uint8_t mark;
};

static_assert(offsetof(NameObject, refs) == 4, "refcount");
static_assert(offsetof(NameObject, mark) == 0x24, "dtor mark");
static_assert(offsetof(HolderObject, name) == 0x1C, "cached name");
static_assert(offsetof(HolderObject, mark) == 0x24, "holder mark");

void onDestroy(void* self, int) {
    static_cast<std::uint8_t*>(self)[0x24] = 0xD1;
}

void* rateView(void* self) {
    return *reinterpret_cast<void**>(static_cast<std::uint8_t*>(self) + 8);
}

void* rateMissing(void*) {
    return nullptr;
}

void* querySelf(void* self) {
    return self;
}

void* g_destroyVtable[4] = {reinterpret_cast<void*>(&onDestroy)};

void __cdecl testFree(void* block) {
    g_freedTag = block == nullptr ? 0u : static_cast<NameObject*>(block)->tag;
}

struct TestXfer {
    void** vtable;
    std::uint32_t version;
    const std::uint8_t* cursor;
    const std::uint8_t* end;
    std::uint32_t consumed;
    std::uint32_t captured;
};

void copyFromStream(TestXfer* self, void* dest, std::uint32_t size) {
    if (self->cursor + size > self->end) {
        return;
    }
    std::memcpy(dest, self->cursor, size);
    self->cursor += size;
    self->consumed += size;
}

void xfer2(TestXfer* self, void* dest) { copyFromStream(self, dest, 2); }
void xfer4(TestXfer* self, void* dest) { copyFromStream(self, dest, 4); }
void xfer12(TestXfer* self, void* dest) { copyFromStream(self, dest, 12); }
void xfer1(TestXfer* self, void* dest) { copyFromStream(self, dest, 1); }

void xferNamed(TestXfer* self, const void*, void* dest, int) {
    self->captured = *static_cast<std::uint32_t*>(dest);
    copyFromStream(self, dest, 4);
}

struct TestModule {
    void** vtable;
    std::uint32_t word;
};

void moduleXfer(TestModule* self, TestXfer* xfer) {
    copyFromStream(xfer, &self->word, 4);
}

std::uint32_t takeHex(std::istream& in) {
    std::uint32_t value = 0;
    in >> std::hex >> value;
    return value;
}

std::string takeToken(std::istream& in) {
    std::string token;
    in >> token;
    return token;
}

std::vector<std::uint8_t> decodeHex(const std::string& hex) {
    std::vector<std::uint8_t> out(hex.size() / 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<std::uint8_t>(std::stoul(hex.substr(i * 2, 2), nullptr, 16));
    }
    return out;
}

std::string drawLog() {
    std::string out = hexU32(static_cast<std::uint32_t>(g_drawCount));
    out += hexBytes(g_draws, static_cast<std::size_t>(g_drawCount) * sizeof(DrawRec));
    return out;
}

void resetDraws() {
    g_drawCount = 0;
    std::memset(g_draws, 0, sizeof(g_draws));
}

void installDrawer() {
    std::memset(g_drawerVtable, 0, sizeof(g_drawerVtable));
    g_drawerVtable[0x30 / 4] = reinterpret_cast<void*>(&onLine);
    g_drawerVtable[0x3C / 4] = reinterpret_cast<void*>(&onLine);
    g_drawerVtable[0x44 / 4] = reinterpret_cast<void*>(&onCircle);
    std::memset(&g_root, 0, sizeof(g_root));
    *reinterpret_cast<void**>(g_drawerObject) = g_drawerVtable;
    g_root.drawer = g_drawerObject;
    g_fx66DebugRoot = &g_root;
}

struct BuiltModule {
    std::uint8_t tmpl[0x40];
    std::uint8_t module[0x20];
    float point[3];
};

BuiltModule buildVolume(std::uint32_t ax, std::uint32_t ay, std::uint32_t az, std::uint32_t bx, std::uint32_t by,
    std::uint32_t bz, std::uint32_t px, std::uint32_t py, std::uint32_t pz) {
    BuiltModule built{};
    std::memcpy(built.tmpl + 0x0C, &ax, 4);
    std::memcpy(built.tmpl + 0x10, &ay, 4);
    std::memcpy(built.tmpl + 0x14, &az, 4);
    std::memcpy(built.tmpl + 0x18, &bx, 4);
    std::memcpy(built.tmpl + 0x1C, &by, 4);
    std::memcpy(built.tmpl + 0x20, &bz, 4);
    *reinterpret_cast<std::uint8_t**>(built.module + 8) = built.tmpl;
    built.point[0] = floatOf(px);
    built.point[1] = floatOf(py);
    built.point[2] = floatOf(pz);
    return built;
}

sim::RandomState stateFrom(const std::string& hex) {
    sim::RandomState state{};
    const auto bytes = decodeHex(hex);
    std::memcpy(state.words.data(), bytes.data(), 24);
    return state;
}

}  // namespace

int main() {
    installDrawer();
    g_fx66Alloc = &testAlloc;
    g_fx66Free = &testFree;
    g_fx66SwarmPrepare = &onPrepare;
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
            const auto word = takeHex(in);
            setFpuControlWord(static_cast<std::uint16_t>(word));
            std::cout << "ok\n";
            continue;
        }
        if (cmd == "box" || cmd == "cyl" || cmd == "line" || cmd == "sphere" || cmd == "spline0") {
            const auto a = takeHex(in);
            const auto b = takeHex(in);
            const auto c = takeHex(in);
            const auto d = takeHex(in);
            const auto e = takeHex(in);
            const auto f = takeHex(in);
            const auto px = takeHex(in);
            const auto py = takeHex(in);
            const auto pz = takeHex(in);
            auto built = buildVolume(a, b, c, d, e, f, px, py, pz);
            if (cmd == "cyl") {
                *reinterpret_cast<float*>(built.module + 0x0C) = floatOf(d);
            }
            resetDraws();
            if (cmd == "box") {
                debugDrawBoxEmissionVolume(built.module, built.point);
            } else if (cmd == "cyl") {
                debugDrawCylinderEmissionVolume(built.module, built.point);
            } else if (cmd == "line" || cmd == "spline0") {
                if (cmd == "spline0") {
                    built.module[0x0C] = 0;
                }
                if (cmd == "line") {
                    debugDrawLineEmissionVolume(built.module, built.point);
                } else {
                    debugDrawSplineEmissionVolume(built.module, built.point);
                }
            } else {
                debugDrawSphereEmissionVolume(built.module, built.point);
            }
            std::cout << drawLog() << '\n';
            continue;
        }
        if (cmd == "point" || cmd == "lightning") {
            float point[3] = {floatOf(takeHex(in)), floatOf(takeHex(in)), floatOf(takeHex(in))};
            resetDraws();
            if (cmd == "point") {
                debugDrawPointEmissionVolume(point);
            } else {
                debugDrawLightningEmissionVolume(point);
            }
            std::cout << drawLog() << '\n';
            continue;
        }
        if (cmd == "splinen") {
            const auto count = takeHex(in);
            const auto px = takeHex(in);
            const auto py = takeHex(in);
            const auto pz = takeHex(in);
            std::vector<std::uint8_t> records(static_cast<std::size_t>(count) * 0x18u);
            for (std::uint32_t i = 0; i < count; ++i) {
                float xyz[3] = {floatOf(takeHex(in)), floatOf(takeHex(in)), floatOf(takeHex(in))};
                std::memcpy(records.data() + static_cast<std::size_t>(i) * 0x18u, xyz, sizeof(xyz));
            }
            std::uint8_t module[0x20] = {};
            module[0x0C] = 1;
            *reinterpret_cast<std::uint8_t**>(module + 0x10) = records.data();
            *reinterpret_cast<std::uint8_t**>(module + 0x14) = records.data() + records.size();
            float point[3] = {floatOf(px), floatOf(py), floatOf(pz)};
            resetDraws();
            debugDrawSplineEmissionVolume(module, point);
            std::cout << drawLog() << '\n';
            continue;
        }
        if (cmd == "swarm") {
            const auto mode = takeHex(in);
            const auto rateBits = takeHex(in);
            const auto tx = takeHex(in);
            const auto ty = takeHex(in);
            const auto tz = takeHex(in);
            const auto count = takeHex(in);
            const auto blob = decodeHex(takeToken(in));
            std::vector<std::uint8_t> records = blob;
            std::uint8_t system[0x110] = {};
            std::memcpy(system + 0xE0, &tx, 4);
            std::memcpy(system + 0xF0, &ty, 4);
            std::memcpy(system + 0x100, &tz, 4);
            std::uint8_t rateData[0x14] = {};
            std::memcpy(rateData + 0x10, &rateBits, 4);
            std::uint8_t view[0x10] = {};
            *reinterpret_cast<std::uint8_t**>(view + 8) = rateData;
            void* rateVt[24] = {};
            rateVt[0x54 / 4] = reinterpret_cast<void*>(&rateView);
            std::uint8_t rateObject[0x10] = {};
            *reinterpret_cast<void**>(rateObject) = rateVt;
            *reinterpret_cast<std::uint8_t**>(rateObject + 8) = view;
            if (mode != 1u) {
                *reinterpret_cast<std::uint8_t**>(system + 0x20) = rateObject;
            }
            if (mode == 2u) {
                rateVt[0x54 / 4] = reinterpret_cast<void*>(&rateMissing);
            }
            std::uint8_t* systemPtr = system;
            std::uint8_t module[0x14] = {};
            *reinterpret_cast<std::uint8_t**>(module + 4) = reinterpret_cast<std::uint8_t*>(&systemPtr);
            auto* begin = records.data();
            auto* end = records.data() + static_cast<std::size_t>(count) * 0x1Cu;
            *reinterpret_cast<std::uint8_t**>(module + 8) = begin;
            *reinterpret_cast<std::uint8_t**>(module + 0x0C) = end;
            integrateSwarmStorageRecords(module);
            const auto newCount = static_cast<std::uint32_t>(
                (*reinterpret_cast<std::uint8_t**>(module + 0x0C) - *reinterpret_cast<std::uint8_t**>(module + 8)) / 0x1C);
            std::cout << hexU32(newCount) << hexBytes(records.data(), static_cast<std::size_t>(newCount) * 0x1Cu) << '\n';
            continue;
        }
        if (cmd == "count") {
            const auto bytes = static_cast<std::int32_t>(takeHex(in));
            std::uint8_t module[0x10] = {};
            auto* begin = reinterpret_cast<std::uint8_t*>(0x1000);
            *reinterpret_cast<std::uint8_t**>(module + 8) = begin;
            *reinterpret_cast<std::uint8_t**>(module + 0x0C) = begin + bytes;
            std::cout << hexU32(static_cast<std::uint32_t>(swarmStorageRecordCount(module))) << '\n';
            continue;
        }
        if (cmd == "addref") {
            const auto which = takeHex(in);
            const auto refs = static_cast<long>(takeHex(in));
            const auto present = takeHex(in);
            NameObject name{};
            name.refs = refs;
            name.tag = 0x11111111u;
            std::uint8_t tmpl[0x50] = {};
            if (present != 0u) {
                if (which == 0u) {
                    *reinterpret_cast<NameObject**>(tmpl + 0x1C) = &name;
                } else if (which == 1u) {
                    *reinterpret_cast<NameObject**>(tmpl + 0x48) = &name;
                } else if (which == 2u) {
                    g_fx66StreakSource = tmpl;
                    *reinterpret_cast<NameObject**>(tmpl + 0x1C) = &name;
                } else {
                    g_fx66TrailGate = tmpl;
                    *reinterpret_cast<NameObject**>(tmpl + 0x1C) = &name;
                }
            } else if (which == 2u) {
                g_fx66StreakSource = tmpl;
            } else if (which == 3u) {
                g_fx66TrailGate = tmpl;
            }
            std::uint8_t module[0x10] = {};
            *reinterpret_cast<std::uint8_t**>(module + 8) = tmpl;
            void* out = reinterpret_cast<void*>(0x1);
            if (which == 0u) {
                swarmDrawAddRefCachedName(module, &out);
            } else if (which == 1u) {
                gpuDrawAddRefCachedName(module, &out);
            } else if (which == 2u) {
                streakTemplateAddRefGlobalName(&out);
            } else {
                trailTemplateAddRefGlobalName(&out);
            }
            const auto tag = out == nullptr ? 0u : static_cast<NameObject*>(out)->tag;
            const auto got = out == nullptr ? 0u : static_cast<std::uint32_t>(static_cast<NameObject*>(out)->refs);
            std::cout << hexU32(tag) << hexU32(got) << '\n';
            continue;
        }
        if (cmd == "assign" || cmd == "resetaux") {
            NameObject previous{};
            previous.vtable = g_destroyVtable;
            previous.refs = static_cast<long>(takeHex(in));
            previous.tag = takeHex(in);
            g_freedTag = 0;
            std::uint8_t particle[0x50] = {};
            *reinterpret_cast<NameObject**>(particle + 0x44) = &previous;
            if (cmd == "assign") {
                NameObject incoming{};
                incoming.tag = takeHex(in);
                incoming.refs = static_cast<long>(takeHex(in));
                void* slot = &incoming;
                assignFxParticleAuxRef(particle, &slot);
                const auto stored = *reinterpret_cast<NameObject**>(particle + 0x44);
                std::cout << hexU32(stored->tag) << hexU32(static_cast<std::uint32_t>(stored->refs))
                          << hexU32(static_cast<std::uint32_t>(previous.refs)) << hexU32(g_freedTag)
                          << hexU32(previous.mark) << '\n';
            } else {
                const auto mode = takeHex(in);
                g_fx66AuxSentinelRef = static_cast<std::int32_t>(takeHex(in));
                if (mode == 1u) {
                    *reinterpret_cast<std::uintptr_t*>(particle + 0x44) = kFxParticleAuxSentinel;
                } else if (mode == 2u) {
                    previous.vtable = nullptr;
                }
                resetFxParticleAuxRef(particle);
                const auto field = *reinterpret_cast<std::uintptr_t*>(particle + 0x44);
                const auto shown = field == kFxParticleAuxSentinel ? static_cast<std::uint32_t>(field) : 1u;
                std::cout << hexU32(shown) << hexU32(static_cast<std::uint32_t>(g_fx66AuxSentinelRef))
                          << hexU32(static_cast<std::uint32_t>(previous.refs)) << hexU32(g_freedTag) << '\n';
            }
            continue;
        }
        if (cmd == "release") {
            const auto armed = static_cast<std::uint8_t>(takeHex(in));
            const auto present = takeHex(in);
            const auto refs = static_cast<long>(takeHex(in));
            NameObject held{};
            held.vtable = g_destroyVtable;
            held.refs = refs;
            held.tag = 0x33333333u;
            g_fx66SwarmArmed = armed;
            g_fx66SwarmHeld = present == 0u ? nullptr : &held;
            releaseSwarmDrawGlobal();
            std::cout << hexU32(g_fx66SwarmArmed) << hexU32(g_fx66SwarmHeld == nullptr ? 0u : 1u)
                      << hexU32(static_cast<std::uint32_t>(held.refs)) << hexU32(held.mark) << '\n';
            continue;
        }
        if (cmd == "exchange") {
            const auto present = takeHex(in);
            const auto refs = static_cast<long>(takeHex(in));
            const auto namePresent = takeHex(in);
            const auto nameRefs = static_cast<long>(takeHex(in));
            NameObject name{};
            name.refs = nameRefs;
            name.tag = 0x44444444u;
            HolderObject object{};
            object.vtable = g_destroyVtable;
            object.refs = refs;
            object.name = namePresent == 0u ? nullptr : &name;
            object.tag = 0x55555555u;
            std::uint8_t tmpl[0x48] = {};
            if (present != 0u) {
                *reinterpret_cast<HolderObject**>(tmpl + 0x44) = &object;
            }
            void* out = reinterpret_cast<void*>(1);
            exchangeGpuTemplateName(tmpl, &out);
            const auto tag = out == nullptr ? 0u : static_cast<NameObject*>(out)->tag;
            const auto nrefs = out == nullptr ? 0u : static_cast<std::uint32_t>(static_cast<NameObject*>(out)->refs);
            std::cout << hexU32(tag) << hexU32(nrefs) << hexU32(static_cast<std::uint32_t>(object.refs))
                      << hexU32(object.mark) << '\n';
            continue;
        }
        if (cmd == "tangent") {
            std::uint8_t module[0x30] = {};
            const auto x = takeHex(in);
            const auto y = takeHex(in);
            const auto z = takeHex(in);
            std::memcpy(module + 0x20, &x, 4);
            std::memcpy(module + 0x24, &y, 4);
            std::memcpy(module + 0x28, &z, 4);
            float out[3] = {};
            copySplineEmissionTangent(module, out);
            std::cout << hexBytes(out, sizeof(out)) << '\n';
            continue;
        }
        if (cmd == "shader" || cmd == "payload" || cmd == "sword") {
            std::uint8_t inner[0x68] = {};
            const auto a = takeHex(in);
            const auto b = takeHex(in);
            const auto c = takeHex(in);
            std::memcpy(inner + 0x4C, &a, 4);
            std::memcpy(inner + 0x54, &b, 4);
            std::memcpy(inner + 0x5C, &c, 4);
            std::memcpy(inner + 0x60, &a, 4);
            std::uint8_t second[0x10] = {};
            *reinterpret_cast<std::uint8_t**>(second + 8) = inner;
            std::uint8_t first[0x8] = {};
            *reinterpret_cast<std::uint8_t**>(first) = second;
            std::uint8_t module[0x8] = {};
            *reinterpret_cast<std::uint8_t**>(module + 4) = first;
            if (cmd == "shader") {
                std::uint32_t out[2] = {};
                copyDrawModuleShaderPair(module, out);
                std::cout << hexU32(out[0]) << hexU32(out[1]) << '\n';
            } else if (cmd == "payload") {
                const auto* got = drawModulePayload(module);
                std::cout << hexU32(*reinterpret_cast<const std::uint32_t*>(got))
                          << hexU32(got == inner + 0x54 ? 1u : 0u) << '\n';
            } else {
                std::cout << hexU32(drawModuleShaderWord(module)) << '\n';
            }
            continue;
        }
        if (cmd == "xfer" || cmd == "xferext") {
            const auto version = takeHex(in);
            const auto stream = decodeHex(takeToken(in));
            const auto moduleCount = takeHex(in);
            const auto auxMode = takeHex(in);
            const auto auxWord = takeHex(in);
            std::vector<std::uint8_t> particle(0x90, 0xAB);
            for (std::size_t off : {0x44u, 0x4Cu, 0x50u, 0x54u, 0x58u, 0x5Cu, 0x60u, 0x64u}) {
                *reinterpret_cast<std::uint32_t*>(particle.data() + off) = 0;
            }
            void* xferSlots[40] = {};
            xferSlots[0x18 / 4] = reinterpret_cast<void*>(&xfer2);
            xferSlots[0x58 / 4] = reinterpret_cast<void*>(&xfer12);
            xferSlots[0x6C / 4] = reinterpret_cast<void*>(&xfer4);
            xferSlots[0x84 / 4] = reinterpret_cast<void*>(&xfer1);
            xferSlots[0x88 / 4] = reinterpret_cast<void*>(&xferNamed);
            TestXfer xfer{};
            xfer.vtable = xferSlots;
            xfer.version = version;
            xfer.cursor = stream.data();
            xfer.end = stream.data() + stream.size();
            void* moduleVt[8] = {};
            moduleVt[3] = reinterpret_cast<void*>(&moduleXfer);
            TestModule modules[6] = {};
            for (auto& module : modules) {
                module.vtable = moduleVt;
            }
            if (cmd == "xferext") {
                if (moduleCount >= 1u) {
                    *reinterpret_cast<TestModule**>(particle.data() + 0x4C) = &modules[0];
                }
                if (moduleCount >= 2u) {
                    *reinterpret_cast<TestModule**>(particle.data() + 0x50) = &modules[1];
                }
                if (moduleCount >= 3u) {
                    *reinterpret_cast<TestModule**>(particle.data() + 0x54) = &modules[2];
                }
                if (moduleCount >= 4u) {
                    *reinterpret_cast<TestModule**>(particle.data() + 0x5C) = &modules[3];
                }
                if (moduleCount >= 5u) {
                    *reinterpret_cast<TestModule**>(particle.data() + 0x58) = &modules[4];
                }
                TestModule* list[1] = {&modules[5]};
                if (moduleCount >= 6u) {
                    *reinterpret_cast<TestModule***>(particle.data() + 0x60) = list;
                    *reinterpret_cast<TestModule***>(particle.data() + 0x64) = list + 1;
                }
                std::uint8_t auxVt[0x98] = {};
                std::memcpy(auxVt + 0x94, &auxWord, 4);
                std::uint8_t aux[0x8] = {};
                *reinterpret_cast<std::uint8_t**>(aux) = auxVt;
                if (auxMode == 1u) {
                    *reinterpret_cast<std::uintptr_t*>(particle.data() + 0x44) = kFxParticleAuxSentinel;
                } else if (auxMode == 2u) {
                    *reinterpret_cast<std::uint32_t*>(aux) = 0;
                    *reinterpret_cast<std::uint8_t**>(particle.data() + 0x44) = aux;
                } else {
                    *reinterpret_cast<std::uint8_t**>(particle.data() + 0x44) = aux;
                }
                xferFxParticleExtended(particle.data(), &xfer);
            } else {
                xferFxParticleCore(particle.data(), &xfer);
            }
            std::cout << hexU32(xfer.consumed) << hexU32(xfer.captured) << hexBytes(particle.data(), particle.size());
            for (std::uint32_t i = 0; i < 6u; ++i) {
                std::cout << hexU32(modules[i].word);
            }
            std::cout << '\n';
            continue;
        }
        if (cmd == "mkphys" || cmd == "mkterr" || cmd == "clbox" || cmd == "clphys" || cmd == "cllife") {
            g_allocFail = takeHex(in) != 0u;
            auto state = stateFrom(takeToken(in));
            std::uint8_t tmpl[0x40];
            std::memset(tmpl, 0x11, sizeof(tmpl));
            const auto nfloat = takeHex(in);
            for (std::uint32_t i = 0; i < nfloat; ++i) {
                const auto at = takeHex(in);
                const auto bits = takeHex(in);
                std::memcpy(tmpl + at, &bits, 4);
            }
            const auto nbytes = takeHex(in);
            for (std::uint32_t i = 0; i < nbytes; ++i) {
                const auto at = takeHex(in);
                tmpl[at] = static_cast<std::uint8_t>(takeHex(in));
            }
            void* block = nullptr;
            NameObject system{};
            system.tag = 0x66666666u;
            system.refs = static_cast<long>(takeHex(in));
            if (cmd == "mkphys") {
                std::uint8_t module[0x10] = {};
                *reinterpret_cast<std::uint8_t**>(module + 8) = tmpl;
                block = createDefaultPhysicsParticleModule(module, &system, state);
            } else if (cmd == "mkterr") {
                void* slot = &system;
                block = createTerrainCollisionModule(tmpl, &slot, state);
            } else if (cmd == "clbox") {
                block = cloneBoxEmissionVolumeTemplate(tmpl);
            } else if (cmd == "clphys") {
                block = cloneDefaultPhysicsTemplate(tmpl);
            } else {
                block = cloneLifeEventTemplate(tmpl);
            }
            std::uint32_t size = 0;
            if (cmd == "mkphys") {
                size = 0x10;
            } else if (cmd == "mkterr") {
                size = 0x14;
            } else if (cmd == "clbox") {
                size = 0x18;
            } else if (cmd == "clphys") {
                size = 0x24;
            } else {
                size = 0x20;
            }
            std::cout << hexBytes(state.words.data(), 24) << hexU32(static_cast<std::uint32_t>(system.refs));
            if (block == nullptr) {
                std::cout << "00\n";
            } else {
                std::cout << "01" << hexBytes(block, size) << '\n';
                delete[] static_cast<std::uint8_t*>(block);
            }
            continue;
        }
        if (cmd == "trail") {
            g_fx66TrailGate = takeHex(in) == 0u ? nullptr : &g_root;
            g_fx66TrailArmed = static_cast<std::uint8_t>(takeHex(in));
            const auto queryResult = takeHex(in);
            std::uint8_t system[0x98] = {};
            void* queryVt[16] = {};
            queryVt[0x2C / 4] = queryResult == 0u ? reinterpret_cast<void*>(&rateMissing)
                                                  : reinterpret_cast<void*>(&querySelf);
            std::uint8_t attachment[0x8] = {};
            *reinterpret_cast<void**>(attachment) = queryVt;
            *reinterpret_cast<std::uint8_t**>(system + 0x90) = attachment;
            std::uint8_t* systemPtr = system;
            std::uint8_t module[0x8] = {};
            *reinterpret_cast<std::uint8_t**>(module + 4) = reinterpret_cast<std::uint8_t*>(&systemPtr);
            beginTrailDrawPass(module);
            std::cout << hexU32(g_fx66TrailArmed) << '\n';
            continue;
        }
        if (cmd == "swarmpass") {
            const auto kind = takeHex(in);
            const auto shader = takeHex(in);
            const auto gate = takeHex(in);
            const auto refs = static_cast<long>(takeHex(in));
            g_fx66SwarmArmed = 0;
            g_fx66SwarmPassGate = gate;
            NameObject name{};
            name.vtable = g_destroyVtable;
            name.refs = refs;
            name.tag = 0x77777777u;
            std::uint8_t tmpl[0x28] = {};
            if (kind != 2u) {
                *reinterpret_cast<NameObject**>(tmpl + 0x20) = &name;
            }
            std::uint8_t systemTmpl[0x54] = {};
            std::memcpy(systemTmpl + 0x50, &shader, 4);
            std::uint8_t system[0x10] = {};
            *reinterpret_cast<std::uint8_t**>(system + 8) = systemTmpl;
            std::uint8_t* systemPtr = system;
            std::uint8_t module[0x10] = {};
            *reinterpret_cast<std::uint8_t**>(module + 4) = reinterpret_cast<std::uint8_t*>(&systemPtr);
            *reinterpret_cast<std::uint8_t**>(module + 8) = tmpl;
            std::uint8_t arg[0x10] = {};
            *reinterpret_cast<std::uint32_t*>(arg + 8) = kind == 0u ? 1u : 3u;
            prepareSwarmDrawPass(module, arg);
            std::cout << hexU32(g_fx66SwarmArmed) << hexU32(static_cast<std::uint32_t>(name.refs))
                      << hexU32(name.mark) << '\n';
            continue;
        }
        if (cmd == "submit") {
            const auto armed = static_cast<std::uint8_t>(takeHex(in));
            const auto primed = static_cast<std::uint8_t>(takeHex(in));
            const auto hasList = takeHex(in);
            const auto count = takeHex(in);
            g_fx66SwarmArmed = armed;
            g_fx66SwarmHeld = &g_root;
            std::uint8_t member[0x8] = {};
            *reinterpret_cast<void**>(member + 4) = reinterpret_cast<void*>(&onMember);
            g_memberByte = 0;
            std::uint8_t mesh[0x40] = {};
            std::memset(mesh, 0x11, sizeof(mesh));
            std::uint8_t scene[0x60] = {};
            *reinterpret_cast<void**>(scene + 0x18) = reinterpret_cast<void*>(&onSubmit);
            *reinterpret_cast<void**>(scene + 0x58) = &g_root;
            g_fx66Scene = scene;
            std::uint8_t module[0x20] = {};
            module[0x10] = primed;
            module[0x19] = 0;
            *reinterpret_cast<std::uint8_t**>(module + 0x0C) = member;
            *reinterpret_cast<std::uint32_t*>(module + 0x18) = hasList;
            *reinterpret_cast<std::uint32_t*>(module + 0x14) = count;
            *reinterpret_cast<std::uint8_t**>(module + 0x1C) = mesh;
            const auto result = submitSwarmDraw(module);
            std::cout << hexU32(result) << hexU32(module[0x10]) << hexU32(module[0x19]) << hexU32(g_memberByte)
                      << hexU32(mesh[0x3C]) << hexBytes(mesh + 0x28, 20) << '\n';
            continue;
        }
        if (cmd == "name") {
            const auto which = takeHex(in);
            auto guard = takeHex(in);
            auto slot = takeHex(in);
            std::uint32_t* got = nullptr;
            switch (which) {
            case 0: got = cacheDefaultAlphaTemplateName(guard, slot); break;
            case 1: got = cacheDefaultColorTemplateName(guard, slot); break;
            case 2: got = cacheDefaultDrawTemplateName(guard, slot); break;
            case 3: got = cacheDefaultPhysicsTemplateName(guard, slot); break;
            case 4: got = cacheDefaultUpdateTemplateName(guard, slot); break;
            case 5: got = cacheGpuDrawTemplateName(guard, slot); break;
            case 6: got = cacheLifeEventTemplateName(guard, slot); break;
            case 7: got = cacheLightningDrawTemplateName(guard, slot); break;
            case 8: got = cacheLightSourceDrawTemplateName(guard, slot); break;
            default: got = cacheSwarmDrawTemplateName(guard, slot); break;
            }
            std::cout << hexU32(guard) << hexU32(slot) << hexU32(got == &slot ? 1u : 0u) << '\n';
            continue;
        }
        std::cout << "error\n";
    }
    return 0;
}
