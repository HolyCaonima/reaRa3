#include "ra3/render/RenderScript9B2.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ra3::render {
namespace {

ScriptHost gHost{};

constexpr std::uint32_t kVariableName = 0x00C5DA48u;
constexpr std::uint32_t kUnknownShader = 0x00C61718u;
constexpr std::uint32_t kUnknownTexture = 0x00C6173Cu;
constexpr std::uint32_t kUnknownDepth = 0x00C61AF0u;
constexpr std::uint32_t kUnknownSize = 0x00C61DA8u;
constexpr std::uint32_t kUnknownCamera = 0x00C61ACCu;
constexpr std::uint32_t kUsageFormat = 0x00BF47B4u;
constexpr std::uint32_t kNullName = 0x00BD12DCu;
constexpr std::uint32_t kElifName = 0x00C60874u;
constexpr std::uint32_t kMatrixOrder = 0x00C600D4u;
constexpr std::uint32_t kD3dOrder = 0x00C600D0u;
constexpr std::uint32_t kGlOrder = 0x00C600CCu;
constexpr std::uint32_t kScopeBlock = 0x00C60078u;
constexpr std::uint32_t kBadOrder = 0x00C60094u;
constexpr std::uint32_t kRenderQuad = 0x00C61380u;
constexpr std::uint32_t kRenderFullQuad = 0x00C61464u;
constexpr std::uint32_t kDefaultStream = 0x009B62B0u;

std::uint32_t load32(const void* base, std::ptrdiff_t offset) noexcept {
    std::uint32_t value = 0;
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, 4);
    return value;
}

void store32(void* base, std::ptrdiff_t offset, std::uint32_t value) noexcept {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, 4);
}

void store8(void* base, std::ptrdiff_t offset, std::uint8_t value) noexcept {
    static_cast<std::uint8_t*>(base)[offset] = value;
}

std::uint8_t* ptr(std::uint32_t address) noexcept {
    return reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(address));
}

std::uint32_t addr(const void* pointer) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

// `mov al, 1` keeps the rest of EAX. Several epilogues do that while EAX still
// holds a blob pointer, so the relativized result is that pointer with byte 1.
std::uint32_t lowOne(const void* pointer) noexcept {
    return (addr(pointer) & 0xFFFFFF00u) | 1u;
}

std::uint32_t bitsOf(float value) noexcept {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, 4);
    return bits;
}

using Call0 = std::uint32_t(__fastcall*)(void*, void*);
using Call1 = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t);
using Call2 = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t);
using Call3 = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t);
using Call5 = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using Cdecl3 = void(__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t);

Call0 method0(std::uint8_t* self, std::uint32_t slot) noexcept {
    return reinterpret_cast<Call0>(load32(ptr(load32(self, 0)), static_cast<std::ptrdiff_t>(slot)));
}

Call1 method1(std::uint8_t* self, std::uint32_t slot) noexcept {
    return reinterpret_cast<Call1>(load32(ptr(load32(self, 0)), static_cast<std::ptrdiff_t>(slot)));
}

Call2 method2(std::uint8_t* self, std::uint32_t slot) noexcept {
    return reinterpret_cast<Call2>(load32(ptr(load32(self, 0)), static_cast<std::ptrdiff_t>(slot)));
}

Call3 method3(std::uint8_t* self, std::uint32_t slot) noexcept {
    return reinterpret_cast<Call3>(load32(ptr(load32(self, 0)), static_cast<std::ptrdiff_t>(slot)));
}

std::uint32_t internKey(std::uint8_t* pool, std::uint32_t name) noexcept {
    if (pool == nullptr) {
        return name;
    }
    return method1(pool, 4)(pool, nullptr, name);
}

void releaseKey(std::uint8_t* pool, std::uint32_t key) noexcept {
    if (pool != nullptr && key != 0u) {
        method1(pool, 8)(pool, nullptr, key);
    }
}

std::uint8_t* allocate(std::uint32_t size) noexcept {
    auto* manager = gHost.manager;
    return reinterpret_cast<std::uint8_t*>(method3(manager, 8)(manager, nullptr, size, kVariableName, 1u));
}

void initString(std::uint8_t* object) noexcept {
    auto* stringPool = gHost.pool;
    store32(object, 0, addr(stringPool));
    store32(object, 4, 0u);
    store8(object, 8, 0u);
}

void initVariable(std::uint8_t* object) noexcept {
    initString(object);
    initString(object + 0x14);
    initString(object + 0x1C);
}

// 0x009B6080, run for real on the emulator side and mirrored here.
std::uint8_t* assignString(std::uint8_t* dest, std::uint8_t* src) noexcept {
    if (dest == src) {
        return dest;
    }
    const auto oldKey = load32(dest, 4);
    if (oldKey != 0u) {
        releaseKey(ptr(load32(dest, 0)), oldKey);
        store32(dest, 4, 0u);
    }
    const auto key = load32(src, 4);
    if (key != 0u) {
        auto* stringPool = ptr(load32(dest, 0));
        const auto stored = stringPool != nullptr ? internKey(stringPool, key) : key;
        store32(dest, 4, stored);
    }
    return dest;
}

// 0x00998440.
void assignText(std::uint8_t* dest, std::uint32_t text) noexcept {
    const auto oldKey = load32(dest, 4);
    if (oldKey != 0u) {
        releaseKey(ptr(load32(dest, 0)), oldKey);
        store32(dest, 4, 0u);
    }
    if (text != 0u) {
        auto* stringPool = ptr(load32(dest, 0));
        const auto stored = stringPool != nullptr ? internKey(stringPool, text) : text;
        store32(dest, 4, stored);
    }
}

void pushDword(std::uint8_t* vec, std::uint32_t value) noexcept {
    if (gHost.reserve != nullptr) {
        gHost.reserve(vec, 1u, 0u);
    }
    const auto size = load32(vec, 0x0C);
    auto* slot = ptr(load32(vec, 0));
    store32(vec, 0x0C, size + 1u);
    if (slot != nullptr) {
        store32(slot, static_cast<std::ptrdiff_t>(size * 4u), value);
    }
}

std::uint32_t pushedIndex(std::uint8_t* context) noexcept {
    return load32(context, 0xC8) - 1u;
}

void logMessage(std::uint32_t format, std::uint32_t text) noexcept {
    if (gHost.log != nullptr) {
        gHost.log(format, text);
    }
}

int signedByteCompare(std::uint8_t left, std::uint8_t right) noexcept {
    const auto a = static_cast<std::int8_t>(left);
    const auto b = static_cast<std::int8_t>(right);
    return a > b ? 1 : (a < b ? -1 : 0);
}

// Same folding as 0x009F3560.
int compareFolded(const char* left, const char* right, std::uint32_t count) noexcept {
    if (count == 0u) {
        return 0;
    }
    while (true) {
        auto a = static_cast<std::uint8_t>(*left++);
        const auto b = static_cast<std::uint8_t>(*right++);
        if (a >= 'A' && a <= 'Z' && b >= 'a' && b <= 'z') {
            a = static_cast<std::uint8_t>(a + 0x20);
        } else if (a >= 'a' && a <= 'z' && b >= 'A' && b <= 'Z') {
            a = static_cast<std::uint8_t>(a - 0x20);
        }
        if (a == 0u || b == 0u || a != b || --count == 0u) {
            return signedByteCompare(a, b);
        }
    }
}

std::uint8_t* makeVariable(std::uint32_t type, std::uint8_t* tokens) noexcept {
    auto* object = allocate(0x24);
    if (object != nullptr) {
        initVariable(object);
    }
    store32(object, 0x0C, 0u);
    store32(object, 0x10, type);
    auto* array = ptr(load32(tokens, 0));
    assignString(object + 0x1C, array + 8);
    assignString(object + 0x14, array + 0x10);
    return object;
}

void pushVariable(std::uint8_t* context, std::uint8_t* object) noexcept {
    pushDword(context + 0xBC, addr(object));
}

std::int32_t towardZeroDiv2(std::int32_t value) noexcept {
    const auto sign = value >> 31;
    return (value - sign) >> 1;
}

void preparePass(std::uint8_t* pass) noexcept {
    // 0x009C4B80 on pass+0xF8, then the optional virtual, then 0x009C4550.
    store32(pass, 0x108, 0xABu);
    auto* nested = ptr(load32(pass, 0xC0));
    if (nested != nullptr) {
        method0(nested, 0x30)(nested, nullptr);
    }
    store32(pass, 0x10C, 0xCDu);
}

void uploadConstants(std::uint8_t* self, std::int32_t count, std::uint32_t mode) noexcept {
    if (count <= 0) {
        return;
    }
    auto* pass = ptr(load32(self, 0x24));
    auto* device = ptr(load32(pass, 0x0C));
    const auto stride = load32(ptr(load32(self, 0x1C)), 0x0C);
    const auto source = load32(self, 4);
    auto* fn = reinterpret_cast<Call5>(load32(ptr(load32(device, 0)), 0x14C));
    fn(device, nullptr, addr(device), mode, static_cast<std::uint32_t>(count), source, stride);
}

std::uint32_t keepRemainder(std::uint8_t* self, std::int32_t remainder) noexcept {
    store32(self, 0x10, static_cast<std::uint32_t>(remainder));
    if (remainder <= 0) {
        store32(self, 8, load32(self, 4));
        return static_cast<std::uint32_t>(remainder);
    }
    const auto stride = load32(ptr(load32(self, 0x1C)), 0x0C);
    const auto bytes = static_cast<std::uint32_t>(remainder) * stride;
    auto* dest = ptr(load32(self, 4));
    auto* source = ptr(load32(self, 8)) - bytes;
    std::memmove(dest, source, bytes);
    store32(self, 8, load32(self, 4) + bytes);
    return load32(self, 4);
}

void clearCount(std::uint8_t* self) noexcept {
    store32(self, 0x10, 0u);
    store32(self, 8, load32(self, 4));
}

std::uint32_t scripted(std::ptrdiff_t offset) noexcept {
    return load32(gHost.manager, offset);
}

void appendLog(std::uint8_t* device, std::uint32_t id, std::uint32_t a, std::uint32_t b, std::uint32_t c) noexcept {
    const auto count = load32(device, 0x300);
    const auto at = 0x304 + static_cast<std::ptrdiff_t>(count * 16u);
    store32(device, at, id);
    store32(device, at + 4, a);
    store32(device, at + 8, b);
    store32(device, at + 12, c);
    store32(device, 0x300, count + 1u);
}

void setPass(std::uint8_t* device, std::uint32_t value) noexcept {
    store32(device, 0x2F0, value);
    store32(device, 0x268, load32(device, 0x268) | 0x40000000u);
}

std::uint32_t drawQuad(std::uint8_t* state, std::uint32_t name, bool full) noexcept {
    if (load32(state, 4) == 0u) {
        return 0u;
    }
    auto* device = ptr(load32(state, 0));
    if ((load32(device, 0x88) & 0xFFu) == 0u) {
        return 0u;
    }
    appendLog(device, 1u, name, 0u, 0u);
    const auto saved = load32(device, 0x2F0);
    appendLog(device, 2u, load32(state, 4), load32(state, 8), 0u);
    setPass(device, 0u);
    appendLog(device, 3u, 1u, 0x0Du, 0u);
    appendLog(device, 3u, 3u, 5u, 0u);
    appendLog(device, 3u, 2u, 0x0Du, 0u);
    const auto width = static_cast<float>(static_cast<std::int32_t>(load32(device, 0x80)));
    const auto height = static_cast<float>(static_cast<std::int32_t>(load32(device, 0x84)));
    const float invW = -1.0f / width;
    const float invH = 1.0f / height;
    const auto x0 = bitsOf(invW - 1.0f);
    const auto y0 = bitsOf(invH - 1.0f);
    const auto y1 = bitsOf(invH + 1.0f);
    const auto x1 = bitsOf(invW + 1.0f);
    if (!full) {
        appendLog(device, 4u, x0, y0, 0u);
        appendLog(device, 4u, bitsOf(invW + 3.0f), y0, 0u);
        appendLog(device, 4u, x0, bitsOf(invH + 3.0f), 0u);
    } else {
        appendLog(device, 4u, x0, y1, 0u);
        appendLog(device, 4u, x0, y0, 0u);
        appendLog(device, 4u, x1, y1, 0u);
        appendLog(device, 4u, x1, y0, 0u);
    }
    appendLog(device, 2u, 0u, 0u, 0u);
    setPass(device, saved);
    return 1u;
}

std::uint32_t copyExtent(std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state) noexcept {
    const auto right = load32(parsed, 0x10);
    const auto bottom = load32(parsed, 0x14);
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    if (right != 0xFFFFFFFFu) {
        if (bottom != 0xFFFFFFFFu) {
            width = right;
            height = bottom;
        } else {
            auto* item = ptr(load32(ptr(load32(context, 0x30)), static_cast<std::ptrdiff_t>(right * 4u)));
            auto* surface = ptr(load32(item, 0x0C));
            width = load32(surface, 0x18);
            height = load32(surface, 0x1C);
        }
    } else if (bottom != 0xFFFFFFFFu) {
        auto* item = ptr(load32(ptr(load32(context, 0x1C)), static_cast<std::ptrdiff_t>(bottom * 4u)));
        const auto queried = method0(item, 4)(item, nullptr);
        auto* surface = ptr(queried);
        width = load32(surface, 0x24);
        height = load32(surface, 0x28);
    } else {
        return 0xFFFFFF00u;
    }
    auto* device = ptr(load32(state, 0));
    store32(device, 0x100, load32(parsed, 8));
    store32(device, 0x104, load32(parsed, 0x0C));
    store32(device, 0x108, width);
    store32(device, 0x10C, height);
    return 1u;
}

std::uint32_t parseCondition(std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens, std::uint32_t flag) noexcept {
    if (load32(tokens, 0x0C) != 3u) {
        return 0u;
    }
    store32(out, 8, 0xFFFFFFFFu);
    store32(out, 0x0C, flag);
    auto* frame = allocate(0x14);
    if (frame != nullptr) {
        initString(frame);
    }
    assignString(frame, ptr(load32(tokens, 0)));
    store32(frame, 0x0C, load32(context, 0x124));
    store8(frame, 0x10, 0u);
    auto* variable = makeVariable(6u, tokens);
    pushVariable(context, variable);
    store32(out, 0x10, pushedIndex(context));
    pushDword(context + 0x12C, addr(frame));
    return 1u;
}

std::uint32_t execCondition(std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* ip, bool invert) noexcept {
    auto* variable = ptr(load32(ptr(load32(context, 0xBC)), static_cast<std::ptrdiff_t>(load32(parsed, 0x10) * 4u)));
    const auto byte = ptr(load32(ptr(load32(variable, 0x0C)), 0x4C))[0];
    const bool take = invert ? byte != 0u : byte == 0u;
    auto* frame = allocate(0x14);
    if (frame != nullptr) {
        initString(frame);
    }
    if (take) {
        const auto target = static_cast<std::int32_t>(load32(parsed, 8));
        if (target >= 0) {
            store32(ip, 0, static_cast<std::uint32_t>(target - 1));
            store32(frame, 0x0C, 1u);
        }
    } else {
        store32(frame, 0x0C, 0u);
    }
    pushDword(context + 0x12C, addr(frame));
    return 1u;
}

void patchOpenFrame(std::uint8_t* context) noexcept {
    const auto size = load32(context, 0x138);
    auto* frame = ptr(load32(ptr(load32(context, 0x12C)), static_cast<std::ptrdiff_t>(size * 4u - 4u)));
    const auto index = load32(frame, 0x0C);
    auto* list = ptr(load32(context, 0x118));
    store32(list, static_cast<std::ptrdiff_t>(index * 40u + 8u), load32(context, 0x124));
    store32(frame, 0x0C, 0xFFFFFFFFu);
}

}  // namespace

void setScriptHost(const ScriptHost& host) noexcept {
    gHost = host;
}

std::uint32_t parseSetIntCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (load32(tokens, 0x0C) != 4u) {
        return 0u;
    }
    auto* variable = makeVariable(4u, tokens);
    pushVariable(context, variable);
    store32(out, 4, pushedIndex(context));
    const auto parsed = parseIntTokens(context, tokens, 3u, out + 8, 1u);
    return parsed == 1u ? 1u : 0u;
}

std::uint32_t parseSetBoolCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (load32(tokens, 0x0C) != 4u) {
        return 0u;
    }
    auto* variable = makeVariable(6u, tokens);
    pushVariable(context, variable);
    store8(out, 8, 0u);
    store32(out, 4, pushedIndex(context));
    const auto text = load32(ptr(load32(tokens, 0)), 0x1C);
    if (compareFolded(reinterpret_cast<const char*>(text), "true", 4u) == 0) {
        store8(out, 8, 1u);
    }
    return 1u;
}

std::uint32_t parseSetFloatCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (static_cast<std::int32_t>(load32(tokens, 0x0C)) < 3) {
        return 0u;
    }
    auto* variable = makeVariable(1u, tokens);
    pushVariable(context, variable);
    store32(out, 4, pushedIndex(context));
    const auto produced = parseFloatTokens(context, tokens, 3u, out + 8, 4u);
    const auto index = static_cast<std::uint32_t>(static_cast<std::int32_t>(produced) - 1);
    if (index > 3u) {
        return 0u;
    }
    static constexpr std::uint32_t kTypes[4] = {3u, 7u, 8u, 9u};
    store32(variable, 0x10, kTypes[index]);
    return 1u;
}

std::uint32_t parseSetDepthCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    (void)context;
    if (static_cast<std::int32_t>(load32(tokens, 0x0C)) < 2) {
        return 0u;
    }
    auto* stringPool = gHost.pool;
    const auto key = stringPool != nullptr ? internKey(stringPool, kNullName) : kNullName;
    const auto matched = static_cast<std::uint8_t>(scripted(0x30));
    releaseKey(stringPool, key);
    std::uint32_t surface = 0xFFFFFFFFu;
    if (matched != 0u) {
        surface = scripted(0x38);
        if (static_cast<std::int32_t>(surface) < 0) {
            logMessage(kUnknownDepth, load32(ptr(load32(tokens, 0)), 0x0C));
            return 0u;
        }
    }
    store32(out, 0x0C, surface);
    store32(out, 8, 0u);
    return lowOne(out);
}

std::uint32_t parseGotoCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (static_cast<std::int32_t>(load32(tokens, 0x0C)) < 2) {
        return 0u;
    }
    store32(out, 8, 0xFFFFFFFFu);
    auto* label = allocate(0x14);
    if (label != nullptr) {
        initString(label);
    }
    store32(label, 0x0C, load32(context, 0x124));
    assignText(label, load32(ptr(load32(tokens, 0)), 0x0C));
    store8(label, 0x10, 0u);
    pushDword(context + 0xF8, addr(label));
    return 1u;
}

std::uint32_t parseBeginEventCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (static_cast<std::int32_t>(load32(tokens, 0x0C)) < 2) {
        return 0u;
    }
    auto* event = allocate(0x0C);
    if (event != nullptr) {
        initString(event);
    }
    assignText(event, load32(ptr(load32(tokens, 0)), 0x0C));
    pushDword(context + 0xD0, addr(event));
    store32(out, 8, 0u);
    store32(out, 0x0C, load32(context, 0xDC) - 1u);
    return lowOne(out);
}

std::uint32_t parseCallProcCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    store32(out, 8, 0xFFFFFFFFu);
    auto* proc = allocate(0x14);
    if (proc != nullptr) {
        initString(proc);
    }
    store32(proc, 0x0C, load32(context, 0x124));
    assignText(proc, load32(ptr(load32(tokens, 0)), 0x0C));
    store8(proc, 0x10, 1u);
    pushDword(context + 0xF8, addr(proc));
    return 1u;
}

std::uint32_t parseSetShaderCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (static_cast<std::int32_t>(load32(tokens, 0x0C)) < 2) {
        return 0u;
    }
    const auto name = load32(ptr(load32(tokens, 0)), 0x0C);
    const auto shader = findShaderByName(context, reinterpret_cast<const char*>(name));
    if (static_cast<std::int32_t>(shader) < 0) {
        logMessage(kUnknownShader, name);
        return 0u;
    }
    const auto count = load32(tokens, 0x0C);
    // The count==3 parse writes its integer into the same slot that starts at -1.
    // A successful parse therefore takes the type-0 finish, not the technique path.
    std::uint32_t parsedSlot = 0xFFFFFFFFu;
    std::uint32_t technique = 0xFFFFFFFFu;
    std::uint32_t variableIndex = 0xFFFFFFFFu;
    if (count == 4u) {
        auto* variable = allocate(0x24);
        if (variable != nullptr) {
            initVariable(variable);
        }
        store32(variable, 0x0C, 0u);
        store32(variable, 0x10, 4u);
        auto* array = ptr(load32(tokens, 0));
        assignString(variable + 0x1C, array + 0x10);
        assignString(variable + 0x14, array + 0x18);
        pushVariable(context, variable);
        variableIndex = pushedIndex(context);
    } else if (count == 3u) {
        if (parseIntTokens(context, tokens, 2u, reinterpret_cast<std::uint8_t*>(&parsedSlot), 1u) != 1u) {
            parsedSlot = 0xFFFFFFFFu;
            const auto text = load32(ptr(load32(tokens, 0)), 0x14);
            technique = scripted(0x34);
            if (static_cast<std::int32_t>(technique) < 0) {
                auto* created = allocate(0x10);
                if (created != nullptr) {
                    initString(created);
                }
                assignText(created, text);
                store32(created, 0x0C, 0xFFFFFFFFu);
                auto* record = ptr(load32(ptr(load32(context, 8)), static_cast<std::ptrdiff_t>(shader * 4u)));
                pushDword(record + 0x18, addr(created));
                technique = load32(record, 0x24) - 1u;
            }
        }
    }
    store32(out, 8, shader);
    store32(out, 0x0C, 0xFFFFFFFFu);
    if (parsedSlot != 0xFFFFFFFFu) {
        store32(out, 0x0C, 0u);
        store32(out, 0x10, parsedSlot);
    } else if (technique != 0xFFFFFFFFu) {
        store32(out, 0x0C, 1u);
        store32(out, 0x10, technique);
    } else if (variableIndex != 0xFFFFFFFFu) {
        store32(out, 0x0C, 2u);
        store32(out, 0x10, variableIndex);
    }
    return lowOne(out);
}

std::uint32_t parseSetShaderOverrideCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (static_cast<std::int32_t>(load32(tokens, 0x0C)) < 3) {
        return 0u;
    }
    const auto name = load32(ptr(load32(tokens, 0)), 0x0C);
    const auto shader = findShaderByName(context, reinterpret_cast<const char*>(name));
    if (static_cast<std::int32_t>(shader) < 0) {
        logMessage(kUnknownShader, name);
        return 0u;
    }
    auto technique = scripted(0x34);
    if (static_cast<std::int32_t>(technique) < 0) {
        auto* created = allocate(0x10);
        if (created != nullptr) {
            initString(created);
        }
        assignText(created, load32(ptr(load32(tokens, 0)), 0x14));
        store32(created, 0x0C, 0xFFFFFFFFu);
        auto* record = ptr(load32(ptr(load32(context, 8)), static_cast<std::ptrdiff_t>(shader * 4u)));
        pushDword(record + 0x18, addr(created));
        technique = load32(record, 0x24) - 1u;
    }
    store32(out, 8, shader);
    store32(out, 0x0C, technique);
    return 1u;
}

std::uint32_t parseSetTextureCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (static_cast<std::int32_t>(load32(tokens, 0x0C)) < 4) {
        return 0u;
    }
    auto* variable = makeVariable(0x0Cu, tokens);
    pushVariable(context, variable);
    const auto name = load32(ptr(load32(tokens, 0)), 0x1C);
    const auto found = findTextureByName(context, reinterpret_cast<const char*>(name));
    if (static_cast<std::int32_t>(found) < 0) {
        logMessage(kUnknownTexture, name);
        return 0u;
    }
    store32(out, 0x0C, found);
    store32(out, 8, pushedIndex(context));
    return 1u;
}

std::uint32_t parseSetToSizeCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (static_cast<std::int32_t>(load32(tokens, 0x0C)) < 4) {
        return 0u;
    }
    auto* variable = makeVariable(7u, tokens);
    pushVariable(context, variable);
    const auto name = load32(ptr(load32(tokens, 0)), 0x1C);
    const auto texture = findTextureByName(context, reinterpret_cast<const char*>(name));
    const auto surface = scripted(0x38);
    if (static_cast<std::int32_t>(texture) < 0 && static_cast<std::int32_t>(surface) < 0) {
        logMessage(kUnknownSize, name);
        return 0u;
    }
    store32(out, 0x0C, texture);
    store32(out, 0x10, surface);
    store32(out, 8, pushedIndex(context));
    return (surface & 0xFFFFFF00u) | 1u;
}

std::uint32_t parseElifCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (load32(tokens, 0x0C) != 3u) {
        return 0u;
    }
    const auto size = load32(context, 0x138);
    auto* frame = ptr(load32(ptr(load32(context, 0x12C)), static_cast<std::ptrdiff_t>(size * 4u - 4u)));
    const auto index = load32(frame, 0x0C);
    store32(ptr(load32(context, 0x118)), static_cast<std::ptrdiff_t>(index * 40u + 8u), load32(context, 0x124));
    store32(frame, 0x0C, load32(context, 0x124));
    store32(out, 8, 0xFFFFFFFFu);
    auto* stringPool = gHost.pool;
    const auto key = stringPool != nullptr ? internKey(stringPool, kElifName) : kElifName;
    const auto matched = static_cast<std::uint8_t>(scripted(0x30));
    releaseKey(stringPool, key);
    store32(out, 0x0C, matched == 0u ? 3u : 2u);
    auto* variable = makeVariable(6u, tokens);
    pushVariable(context, variable);
    store32(out, 0x10, pushedIndex(context));
    return lowOne(out);
}

std::uint32_t parseIfNotCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    return parseCondition(context, out, tokens, 1u);
}

std::uint32_t parseIfCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    return parseCondition(context, out, tokens, 0u);
}

std::uint32_t parseEndIfCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (load32(tokens, 0x0C) != 1u) {
        return addr(tokens) & 0xFFFFFF00u;
    }
    patchOpenFrame(context);
    store32(out, 8, 0xFFFFFFFFu);
    store32(out, 0x0C, 5u);
    auto* frames = context + 0x12C;
    const auto size = load32(frames, 0x0C);
    if (static_cast<std::int32_t>(size) > 0) {
        store32(frames, 0x0C, size - 1u);
        if (gHost.shrink != nullptr) {
            gHost.shrink(frames);
        }
    }
    return 1u;
}

std::uint32_t parseSetCameraCommand(std::uint8_t* command, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (static_cast<std::int32_t>(load32(tokens, 0x0C)) < 2) {
        const auto text = method0(command, 8)(command, nullptr);
        logMessage(kUsageFormat, text);
        return 0u;
    }
    const auto name = load32(ptr(load32(tokens, 0)), 0x0C);
    const auto camera = scripted(0x3C);
    if (static_cast<std::int32_t>(camera) < 0) {
        logMessage(kUnknownCamera, name);
        return 0u;
    }
    store32(out, 8, camera);
    parseIntTokens(context, tokens, 2u, out + 0x0C, 3u);
    return 1u;
}

std::uint32_t execRenderQuadCommand(std::uint8_t*, std::uint8_t*, std::uint8_t*, std::uint8_t* state, std::uint8_t*) noexcept {
    return drawQuad(state, kRenderQuad, false);
}

std::uint32_t execRenderFullQuadCommand(std::uint8_t*, std::uint8_t*, std::uint8_t*, std::uint8_t* state, std::uint8_t*) noexcept {
    return drawQuad(state, kRenderFullQuad, true);
}

std::uint32_t execSetViewportCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t*) noexcept {
    return copyExtent(context, parsed, state);
}

std::uint32_t execSetScissorCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t*) noexcept {
    return copyExtent(context, parsed, state);
}

std::uint32_t execSetCameraCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t*) noexcept {
    auto* camera = ptr(load32(ptr(load32(context, 0x6C)), static_cast<std::ptrdiff_t>(load32(parsed, 8) * 4u)));
    auto* device = ptr(load32(state, 0));
    auto* fn = reinterpret_cast<Call5>(load32(ptr(load32(context, 0)), 0x28));
    fn(context, nullptr, addr(device), load32(camera, 0x0C), load32(parsed, 0x0C), load32(device, 0x80), load32(device, 0x84));
    return 1u;
}

std::uint32_t execSetToSizeCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t*) noexcept {
    auto* variable = ptr(load32(ptr(load32(context, 0xBC)), static_cast<std::ptrdiff_t>(load32(parsed, 8) * 4u)));
    auto* param = ptr(load32(variable, 0x0C));
    if (param == nullptr) {
        return lowOne(variable);
    }
    float width = 0.0f;
    float height = 0.0f;
    const auto texture = static_cast<std::int32_t>(load32(parsed, 0x0C));
    if (texture >= 0) {
        auto* item = ptr(load32(ptr(load32(context, 0x1C)), static_cast<std::ptrdiff_t>(static_cast<std::uint32_t>(texture) * 4u)));
        const auto queried = method0(item, 4)(item, nullptr);
        auto* surface = ptr(queried);
        if (surface != nullptr) {
            width = static_cast<float>(static_cast<std::int32_t>(load32(surface, 0x24)));
            height = static_cast<float>(static_cast<std::int32_t>(load32(surface, 0x28)));
        }
    } else {
        const auto surfaceIndex = static_cast<std::int32_t>(load32(parsed, 0x10));
        if (surfaceIndex >= 0) {
            auto* item = ptr(load32(ptr(load32(context, 0x30)), static_cast<std::ptrdiff_t>(static_cast<std::uint32_t>(surfaceIndex) * 4u)));
            auto* surface = ptr(load32(item, 0x0C));
            if (surface != nullptr) {
                width = static_cast<float>(static_cast<std::int32_t>(load32(surface, 0x18)));
                height = static_cast<float>(static_cast<std::int32_t>(load32(surface, 0x1C)));
            }
        }
    }
    auto* slot = ptr(load32(param, 0x50));
    store32(param, 0x4C, addr(slot));
    store32(slot, 0, bitsOf(width));
    store32(slot, 4, bitsOf(height));
    store8(param, 0x58, 1u);
    auto* block = ptr(load32(param, 0x34));
    store32(block, 0x108, load32(block, 0x108) | load32(param, 0x40));
    store32(block, 0x10C, load32(block, 0x10C) | load32(param, 0x44));
    return lowOne(block);
}

std::uint32_t execIfNotCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t* ip) noexcept {
    return execCondition(context, parsed, ip, true);
}

std::uint32_t execIfCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t* ip) noexcept {
    return execCondition(context, parsed, ip, false);
}

std::uint32_t execEndIfCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t*, std::uint8_t*, std::uint8_t*) noexcept {
    const auto size = load32(context, 0x138);
    auto* frame = ptr(load32(ptr(load32(context, 0x12C)), static_cast<std::ptrdiff_t>(size * 4u - 4u)));
    if (frame != nullptr) {
        const auto key = load32(frame, 4);
        if (key != 0u) {
            releaseKey(ptr(load32(frame, 0)), key);
            store32(frame, 4, 0u);
        }
        store32(frame, 0, 0u);
        auto* manager = gHost.manager;
        method2(manager, 0x0C)(manager, nullptr, addr(frame), 0u);
    }
    auto* frames = context + 0x12C;
    const auto count = load32(frames, 0x0C);
    if (static_cast<std::int32_t>(count) > 0) {
        store32(frames, 0x0C, count - 1u);
        if (gHost.shrink != nullptr) {
            gHost.shrink(frames);
        }
    }
    return 1u;
}

std::uint32_t flushConstantsHalf(std::uint8_t* self) noexcept {
    auto* pass = ptr(load32(self, 0x24));
    preparePass(pass);
    const auto count = static_cast<std::int32_t>(load32(self, 0x10));
    const auto half = towardZeroDiv2(count);
    uploadConstants(self, half, 2u);
    if (half > 0) {
        return keepRemainder(self, count - half * 2);
    }
    return 0u;
}

std::uint32_t flushConstantsAll(std::uint8_t* self) noexcept {
    preparePass(ptr(load32(self, 0x24)));
    const auto count = static_cast<std::int32_t>(load32(self, 0x10));
    uploadConstants(self, count, 1u);
    if (count > 0) {
        clearCount(self);
    }
    return 0u;
}

std::uint32_t flushConstantsMinusOne(std::uint8_t* self) noexcept {
    preparePass(ptr(load32(self, 0x24)));
    const auto count = static_cast<std::int32_t>(load32(self, 0x10)) - 1;
    uploadConstants(self, count, 3u);
    if (count > 0) {
        clearCount(self);
    }
    return 0u;
}

std::uint32_t flushConstantsThird(std::uint8_t* self) noexcept {
    preparePass(ptr(load32(self, 0x24)));
    const auto count = static_cast<std::int32_t>(load32(self, 0x10));
    const auto third = count / 3;
    uploadConstants(self, third, 4u);
    if (third > 0) {
        return keepRemainder(self, count - third * 3);
    }
    return 0u;
}

std::uint32_t flushConstantsMinusTwoMode5(std::uint8_t* self) noexcept {
    preparePass(ptr(load32(self, 0x24)));
    const auto count = static_cast<std::int32_t>(load32(self, 0x10)) - 2;
    uploadConstants(self, count, 5u);
    if (count > 0) {
        clearCount(self);
    }
    return 0u;
}

std::uint32_t flushConstantsMinusTwoMode6(std::uint8_t* self) noexcept {
    preparePass(ptr(load32(self, 0x24)));
    const auto count = static_cast<std::int32_t>(load32(self, 0x10)) - 2;
    uploadConstants(self, count, 6u);
    if (count > 0) {
        clearCount(self);
    }
    return 0u;
}

std::uint32_t dispatchDirtyState(std::uint8_t* self, std::uint32_t maskA, std::uint32_t maskB) noexcept {
    auto* primary = ptr(load32(self, 0x74));
    if (primary != nullptr) {
        const auto hit = (load32(primary, 0x30) & maskA) | (load32(primary, 0x34) & maskB);
        if (hit != 0u) {
            method2(primary, 0x10)(primary, nullptr, maskA, maskB);
        }
    }
    auto* node = ptr(load32(self, 0x24));
    if (node != nullptr) {
        node -= 8;
        while (node != nullptr) {
            const auto hit = (load32(node, 0x18) & maskA) | (load32(node, 0x1C) & maskB);
            if (hit != 0u) {
                auto* fn = reinterpret_cast<Cdecl3>(load32(node, 0x20));
                fn(addr(node), load32(self, 0x60), load32(node, 0x28));
            }
            auto* next = ptr(load32(node, 0x0C));
            if (next == nullptr) {
                break;
            }
            node = next - 8;
        }
    }
    auto* device = ptr(load32(self, 0x60));
    const auto current = load32(self, 0x68);
    const auto applied = load32(self, 0x6C);
    if (current != applied) {
        if (applied != 0xFFFFFFFFu) {
            auto* endFn = reinterpret_cast<Call1>(load32(ptr(load32(device, 0)), 0x108));
            endFn(device, nullptr, addr(device));
        }
        auto* setFn = reinterpret_cast<Call2>(load32(ptr(load32(device, 0)), 0x100));
        setFn(device, nullptr, addr(device), current);
        store32(self, 0x6C, current);
    } else {
        auto* fn = reinterpret_cast<Call1>(load32(ptr(load32(device, 0)), 0x104));
        fn(device, nullptr, addr(device));
    }
    return 0u;
}

std::uint32_t classifyPrimitive(std::uint8_t*, std::uint8_t* record) noexcept {
    const auto kind = load32(record, 8);
    const auto sub = load32(record, 0x0C);
    if (kind > 4u) {
        return 1u;
    }
    if (kind == 0u) {
        if (sub == 1u) {
            return 6u;
        }
        if (sub == 2u) {
            return 4u;
        }
        if (sub == 3u) {
            return 3u;
        }
        return 1u;
    }
    if (kind == 1u) {
        if (sub != 3u) {
            return 1u;
        }
        const auto sub2 = load32(record, 0x14);
        if (sub2 == 2u) {
            return 7u;
        }
        if (sub2 == 3u) {
            return 8u;
        }
        if (sub2 == 4u) {
            return 9u;
        }
        return 1u;
    }
    if (kind == 2u || kind == 3u) {
        return sub == 3u ? 0x0Bu : 1u;
    }
    return sub == 5u ? 0x0Cu : 1u;
}

std::uint32_t readMatrixOrder(std::uint8_t* self) noexcept {
    store8(self, 0x190, 0u);
    std::uint32_t text = 0;
    const auto first = method2(self, 0x14)(self, nullptr, kMatrixOrder, addr(&text));
    if ((first & 0xFFu) == 1u) {
        if (_stricmp(reinterpret_cast<const char*>(text), "d3d") == 0) {
            store8(self, 0x190, 0u);
        } else if (_stricmp(reinterpret_cast<const char*>(text), "gl") == 0) {
            store8(self, 0x190, 1u);
        } else {
            logMessage(kBadOrder, text);
        }
    }
    store32(self, 0x18C, 0u);
    const auto second = method2(self, 0x14)(self, nullptr, kScopeBlock, addr(self + 0x18C));
    if ((second & 0xFFu) == 0u) {
        store32(self, 0x18C, 0u);
    }
    return second;
}

std::uint32_t selectConstantRoutine(std::uint8_t* self, std::uint32_t kind, std::uint32_t, std::uint8_t* info) noexcept {
    const auto index = static_cast<std::int32_t>(kind) - 3;
    if (index < 0 || index > 9) {
        return 0u;
    }
    const bool flag = load32(info, 0x18) > 1u;
    if (index == 2 || index == 7) {
        return 0u;
    }
    if (index == 9) {
        return flag ? 0u : 0x009BAE70u;
    }
    std::uint32_t set = 0;
    std::uint32_t clear = 0;
    if (index == 0) {
        set = 0x009BADA0u;
        clear = 0x009BADD0u;
    } else if (index == 1) {
        set = 0x009BAE20u;
        clear = 0x009BAE50u;
    } else if (index == 3) {
        set = 0x006870A0u;
        clear = 0x009BAE00u;
    } else if (index == 8) {
        if (static_cast<std::uint8_t*>(self)[0x190] != 0u) {
            set = 0x009BAEF0u;
            clear = 0x009BAF40u;
        } else {
            set = 0x009BAEC0u;
            clear = 0x009BAF20u;
        }
    } else {
        set = 0x009BAF60u;
        clear = 0x009BAF90u;
    }
    return flag ? set : clear;
}

std::uint32_t bindDynamicStream(std::uint8_t* self, std::uint8_t* source, std::uint8_t* callback) noexcept {
    store32(self, 0x1C, addr(source));
    store32(self, 0x20, callback != nullptr ? addr(callback) : kDefaultStream);
    store32(self, 0x18, load32(self, 0x14) * load32(source, 0x0C));
    store32(self, 0x10, 0u);
    auto* object = ptr(load32(self, 0x2C));
    if (load32(object, 0x24) == 0u && load32(self, 0x28) == 0u) {
        store32(self, 4, 0u);
        store32(self, 8, 0u);
        store32(self, 0x0C, 0u);
        return 0u;
    }
    const auto base = method1(object, 0x0C)(object, nullptr, 0u);
    const auto span = load32(object, 0x10) * load32(object, 0x14);
    store32(self, 4, base);
    store32(self, 8, base);
    store32(self, 0x0C, base + span);
    return base;
}

}  // namespace ra3::render
