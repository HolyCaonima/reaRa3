#include "ra3/render/RenderScript9B.h"

#include <cstddef>
#include <cstring>

namespace ra3::render {
namespace {

ScriptServices gServices{};

constexpr std::uint32_t kClearWhite = 0x3F800000u;
constexpr std::uint32_t kDefaultStream = 0x009B62B0u;
constexpr std::uint32_t kUnknownShader = 0x00C61718u;
constexpr std::uint32_t kUnknownTexture = 0x00C6173Cu;
constexpr std::uint32_t kBadTarget = 0x00C60BC8u;
constexpr std::uint32_t kTargetRange = 0x00C60B98u;
constexpr std::uint32_t kBadConstant = 0x00C5EF44u;
constexpr std::uint32_t kFloatFormat = 0x00BD615Cu;
constexpr std::uint32_t kIntFormat = 0x00BDD2E4u;

std::uint32_t load32(const void* base, std::ptrdiff_t offset) noexcept {
    std::uint32_t value = 0;
    std::memcpy(&value, static_cast<const std::uint8_t*>(base) + offset, 4);
    return value;
}

std::uint8_t load8(const void* base, std::ptrdiff_t offset) noexcept {
    return static_cast<const std::uint8_t*>(base)[offset];
}

void store32(void* base, std::ptrdiff_t offset, std::uint32_t value) noexcept {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, 4);
}

void store8(void* base, std::ptrdiff_t offset, std::uint8_t value) noexcept {
    static_cast<std::uint8_t*>(base)[offset] = value;
}

std::uint8_t* ptr(std::uint32_t address) noexcept {
    return reinterpret_cast<std::uint8_t*>(address);
}

std::uint32_t addr(const void* pointer) noexcept {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t lowByte(std::uint32_t value, std::uint32_t byte) noexcept {
    return (value & 0xFFFFFF00u) | (byte & 0xFFu);
}

using Call0 = std::uint32_t(__fastcall*)(void*, void*);
using Call1 = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t);
using Call2 = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t);
using Call3 = std::uint32_t(__fastcall*)(void*, void*, std::uint32_t, std::uint32_t, std::uint32_t);

Call0 slot0(std::uint32_t vtable, std::uint32_t offset) noexcept {
    return reinterpret_cast<Call0>(load32(ptr(vtable), static_cast<std::ptrdiff_t>(offset)));
}

Call1 slot1(std::uint32_t vtable, std::uint32_t offset) noexcept {
    return reinterpret_cast<Call1>(load32(ptr(vtable), static_cast<std::ptrdiff_t>(offset)));
}

Call2 slot2(std::uint32_t vtable, std::uint32_t offset) noexcept {
    return reinterpret_cast<Call2>(load32(ptr(vtable), static_cast<std::ptrdiff_t>(offset)));
}

Call3 slot3(std::uint32_t vtable, std::uint32_t offset) noexcept {
    return reinterpret_cast<Call3>(load32(ptr(vtable), static_cast<std::ptrdiff_t>(offset)));
}

bool isDigit(char ch) noexcept {
    return ch >= '0' && ch <= '9';
}

// Subset of sscanf("%f") used by the script converter: optional sign, digits,
// optional fraction. Values in the oracle are exact in binary32.
bool scanFloat(const char* text, std::uint8_t* dest) noexcept {
    if (text == nullptr) {
        return false;
    }
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    int sign = 1;
    if (*text == '+' || *text == '-') {
        if (*text == '-') {
            sign = -1;
        }
        ++text;
    }
    if (*text != '.' && !isDigit(*text)) {
        return false;
    }
    std::uint32_t whole = 0;
    bool any = false;
    while (isDigit(*text)) {
        whole = whole * 10u + static_cast<std::uint32_t>(*text - '0');
        ++text;
        any = true;
    }
    std::uint32_t frac = 0;
    std::uint32_t scale = 1;
    if (*text == '.') {
        ++text;
        while (isDigit(*text)) {
            frac = frac * 10u + static_cast<std::uint32_t>(*text - '0');
            scale *= 10u;
            ++text;
            any = true;
        }
    }
    if (!any) {
        return false;
    }
    double magnitude = static_cast<double>(whole);
    if (scale > 1u) {
        magnitude += static_cast<double>(frac) / static_cast<double>(scale);
    }
    if (sign < 0) {
        magnitude = -magnitude;
    }
    const float value = static_cast<float>(magnitude);
    std::memcpy(dest, &value, 4);
    return true;
}

bool scanInt(const char* text, std::uint8_t* dest) noexcept {
    if (text == nullptr) {
        return false;
    }
    while (*text == ' ' || *text == '\t') {
        ++text;
    }
    int sign = 1;
    if (*text == '+' || *text == '-') {
        if (*text == '-') {
            sign = -1;
        }
        ++text;
    }
    if (!isDigit(*text)) {
        return false;
    }
    std::int32_t value = 0;
    while (isDigit(*text)) {
        value = value * 10 + (*text - '0');
        ++text;
    }
    if (sign < 0) {
        value = -value;
    }
    std::memcpy(dest, &value, 4);
    return true;
}

const char* tokenText(std::uint8_t* tokens, std::uint32_t index) noexcept {
    const auto array = ptr(load32(tokens, 0));
    return reinterpret_cast<const char*>(load32(array, static_cast<std::ptrdiff_t>(index * 8u + 4u)));
}

std::uint32_t convertTokens(std::uint8_t* context, std::uint8_t* tokens, std::uint32_t start, std::uint8_t* dest, std::uint32_t count, bool asFloat) noexcept {
    if (static_cast<std::int32_t>(count) <= 0) {
        return 0;
    }
    std::uint32_t produced = 0;
    while (produced < count) {
        const std::uint32_t index = start + produced;
        if (static_cast<std::int32_t>(index) >= static_cast<std::int32_t>(load32(tokens, 0x0C))) {
            break;
        }
        const char* text = tokenText(tokens, index);
        if (text[0] == '$') {
            const auto vtable = load32(context, 0);
            const auto method = slot2(vtable, asFloat ? 0x80u : 0x7Cu);
            const auto ok = method(context, nullptr, addr(text + 1), addr(dest));
            if ((ok & 0xFFu) == 0u) {
                if (gServices.log != nullptr) {
                    gServices.log(kBadConstant, addr(text));
                }
                return 0;
            }
        } else if (asFloat) {
            if (!scanFloat(text, dest)) {
                return 0;
            }
        } else if (!scanInt(text, dest)) {
            return 0;
        }
        ++produced;
        dest += 4;
    }
    return produced;
}

std::uint32_t findNamed(std::uint8_t* self, const char* name, std::uint32_t countOffset, std::uint32_t arrayOffset, bool texture) noexcept {
    std::uint32_t index = 0;
    if (static_cast<std::int32_t>(load32(self, static_cast<std::ptrdiff_t>(countOffset))) <= 0) {
        return 0xFFFFFFFFu;
    }
    for (;;) {
        auto* pool = gServices.stringPool;
        std::uint32_t key = 0;
        if (name != nullptr && pool != nullptr) {
            const auto vtable = load32(pool, 0);
            key = slot1(vtable, 4)(pool, nullptr, addr(name));
        }
        auto* entry = ptr(load32(ptr(load32(self, static_cast<std::ptrdiff_t>(arrayOffset))), static_cast<std::ptrdiff_t>(index * 4u)));
        const std::uint32_t entryKey = load32(entry, texture ? 8 : 4);
        bool matched = false;
        if (entryKey == key) {
            matched = true;
        } else if (texture) {
            auto* object = ptr(load32(entry, 4));
            if (object == pool && object != nullptr) {
                const auto vtable = load32(object, 0);
                matched = (slot2(vtable, 0x0C)(object, nullptr, entryKey, key) & 0xFFu) != 0u;
            } else if (entryKey != 0u && key != 0u && gServices.nameEquals != nullptr) {
                matched = (gServices.nameEquals(entry + 4, entryKey, key) & 0xFFu) != 0u;
            }
        } else {
            auto* object = ptr(load32(entry, 0));
            if (object == pool && object != nullptr) {
                const auto vtable = load32(object, 0);
                matched = (slot2(vtable, 0x0C)(object, nullptr, entryKey, key) & 0xFFu) != 0u;
            } else if (entryKey != 0u && key != 0u && gServices.nameEquals != nullptr) {
                matched = (gServices.nameEquals(entry, entryKey, key) & 0xFFu) != 0u;
            }
        }
        if (key != 0u && pool != nullptr) {
            const auto vtable = load32(pool, 0);
            slot1(vtable, 8)(pool, nullptr, key);
        }
        if (matched) {
            return index;
        }
        ++index;
        if (static_cast<std::int32_t>(index) >= static_cast<std::int32_t>(load32(self, static_cast<std::ptrdiff_t>(countOffset)))) {
            return 0xFFFFFFFFu;
        }
    }
}

void pushVector(std::uint8_t* vec, std::uint32_t value) noexcept {
    if (gServices.vectorReserve != nullptr) {
        gServices.vectorReserve(vec, 1u, 0u);
    }
    const auto size = load32(vec, 0x0C);
    auto* slot = ptr(load32(vec, 0));
    const auto grown = size + 1u;
    store32(vec, 0x0C, grown);
    if (slot != nullptr) {
        store32(slot, static_cast<std::ptrdiff_t>(size * 4u), value);
    }
}

std::uint32_t applyNamed(std::uint8_t* self, std::uint32_t name, std::uint32_t value, std::uint32_t secondSlot) noexcept {
    auto* device = ptr(load32(self, 0x38));
    auto vtable = load32(device, 0);
    auto found = slot3(vtable, 0x24)(ptr(vtable), nullptr, addr(device), 0u, name);
    if (found == 0u) {
        return 0;
    }
    device = ptr(load32(self, 0x38));
    vtable = load32(device, 0);
    const auto result = slot3(vtable, secondSlot)(ptr(vtable), nullptr, addr(device), found, value);
    if (static_cast<std::int32_t>(result) < 0) {
        return lowByte(result, 0);
    }
    return lowByte(result, 1);
}

std::uint32_t releaseSurface(std::uint8_t* self, std::uint32_t slot) noexcept {
    if (load32(self, 8) == 0u) {
        return 0;
    }
    auto* object = ptr(load32(self, 8));
    const auto vtable = load32(object, 0);
    return slot1(vtable, slot)(ptr(vtable), nullptr, addr(object));
}

std::uint32_t passRow(std::uint8_t* self) noexcept {
    const auto pass = load32(self, 0x3C);
    const auto scaled = pass + pass * 4u;
    auto* table = ptr(load32(self, 0x174));
    return load32(table, static_cast<std::ptrdiff_t>(scaled * 4u));
}

std::uint32_t selectConstant(std::uint8_t* self, std::uint32_t index, std::uint32_t compared) noexcept {
    auto* row = ptr(passRow(self));
    const auto value = load32(row, static_cast<std::ptrdiff_t>(index * 4u));
    auto* block = self + 0x48;
    std::uint32_t result = index;
    if (index != compared) {
        const auto inner = load32(block, 0x10);
        store32(block, 0x68, index);
        if (inner != 0u && gServices.refreshConstants != nullptr) {
            result = gServices.refreshConstants(ptr(inner), addr(block));
        }
    }
    store32(block, 0x74, value);
    return result;
}

std::uint32_t clearWithMask(std::uint8_t* state, std::uint32_t mask) noexcept {
    auto* device = ptr(load32(state, 0));
    const auto stencil = load32(state, 0x24);
    const auto depth = load32(state, 0x20);
    const auto color = addr(state + 0x10);
    const auto called = gServices.clearSurface != nullptr ? gServices.clearSurface(device, mask, color, depth, stencil) : 0u;
    return lowByte(called, 1);
}

void markParamDirty(std::uint8_t* param, std::uint8_t* slot, std::uint32_t storedWord, bool asByte) noexcept {
    store32(param, 0x4C, addr(slot));
    if (asByte) {
        store8(slot, 0, static_cast<std::uint8_t>(storedWord));
    } else {
        store32(slot, 0, storedWord);
    }
    auto* block = ptr(load32(param, 0x34));
    store8(param, 0x58, 1);
    store32(block, 0x108, load32(block, 0x108) | load32(param, 0x40));
    store32(block, 0x10C, load32(block, 0x10C) | load32(param, 0x44));
}

}  // namespace

void setScriptServices(const ScriptServices& services) noexcept {
    gServices = services;
}

std::uint32_t parseFloatTokens(std::uint8_t* context, std::uint8_t* tokens, std::uint32_t start, std::uint8_t* dest, std::uint32_t count) noexcept {
    return convertTokens(context, tokens, start, dest, count, true);
}

std::uint32_t parseIntTokens(std::uint8_t* context, std::uint8_t* tokens, std::uint32_t start, std::uint8_t* dest, std::uint32_t count) noexcept {
    return convertTokens(context, tokens, start, dest, count, false);
}

std::uint32_t findShaderByName(std::uint8_t* self, const char* name) noexcept {
    return findNamed(self, name, 0x14u, 8u, false);
}

std::uint32_t findTextureByName(std::uint8_t* self, const char* name) noexcept {
    return findNamed(self, name, 0x28u, 0x1Cu, true);
}

std::uint32_t parseClearCommand(std::uint8_t*, std::uint8_t*, std::uint8_t*, std::uint8_t* tokens) noexcept {
    const auto count = load32(tokens, 0x0C);
    const std::uint32_t enough = static_cast<std::int32_t>(count) >= 1 ? 1u : 0u;
    return lowByte(addr(tokens), enough);
}

std::uint32_t parseClearTargetCommand(std::uint8_t*, std::uint8_t*, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (static_cast<std::int32_t>(load32(tokens, 0x0C)) < 2) {
        return lowByte(addr(tokens), 0);
    }
    const char* text = reinterpret_cast<const char*>(load32(ptr(load32(tokens, 0)), 0x0C));
    std::int32_t parsed = -1;
    auto* local = reinterpret_cast<std::uint8_t*>(&parsed);
    if (!scanInt(text, local)) {
        const auto logged = gServices.log != nullptr ? gServices.log(kBadTarget, 0u) : 0u;
        return lowByte(logged, 0);
    }
    if (static_cast<std::uint32_t>(parsed) > 3u) {
        const auto logged = gServices.log != nullptr ? gServices.log(kTargetRange, static_cast<std::uint32_t>(parsed)) : 0u;
        return lowByte(logged, 0);
    }
    store32(out, 8, static_cast<std::uint32_t>(parsed));
    return lowByte(static_cast<std::uint32_t>(parsed), 1);
}

std::uint32_t parseSetClearColorCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    const auto parsed = parseFloatTokens(context, tokens, 1u, out + 8, 4u);
    return lowByte(parsed, parsed == 4u ? 1u : 0u);
}

std::uint32_t parseSetClearDepthCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    const auto parsed = parseFloatTokens(context, tokens, 1u, out + 8, 1u);
    if (parsed != 1u) {
        return lowByte(parsed, 0);
    }
    if (load32(tokens, 0x0C) != 3u) {
        store32(out, 4, 0u);
        return lowByte(parsed, 1);
    }
    const auto stencil = parseIntTokens(context, tokens, 2u, out + 4, 1u);
    if (stencil != 1u) {
        return lowByte(stencil, 0);
    }
    return lowByte(stencil, 1);
}

std::uint32_t parseRenderQuadCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    store32(out, 8, kClearWhite);
    store32(out, 0x0C, kClearWhite);
    store32(out, 0x10, kClearWhite);
    store32(out, 0x14, kClearWhite);
    if (load32(tokens, 0x0C) != 5u) {
        return lowByte(addr(tokens), 1);
    }
    const auto parsed = parseFloatTokens(context, tokens, 1u, out + 8, 4u);
    if (parsed != 4u) {
        return lowByte(parsed, 0);
    }
    return lowByte(parsed, 1);
}

std::uint32_t parseClearShaderOverrideCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (static_cast<std::int32_t>(load32(tokens, 0x0C)) < 2) {
        return lowByte(0, 0);
    }
    const char* name = reinterpret_cast<const char*>(load32(ptr(load32(tokens, 0)), 0x0C));
    const auto found = findShaderByName(context, name);
    if (static_cast<std::int32_t>(found) < 0) {
        const auto logged = gServices.log != nullptr ? gServices.log(kUnknownShader, addr(name)) : 0u;
        return lowByte(logged, 0);
    }
    store32(out, 8, found);
    return lowByte(found, 1);
}

std::uint32_t parseCastTextureCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (static_cast<std::int32_t>(load32(tokens, 0x0C)) < 2) {
        return lowByte(0, 0);
    }
    const char* name = reinterpret_cast<const char*>(load32(ptr(load32(tokens, 0)), 0x0C));
    const auto found = findTextureByName(context, name);
    if (static_cast<std::int32_t>(found) < 0) {
        const auto logged = gServices.log != nullptr ? gServices.log(kUnknownTexture, addr(name)) : 0u;
        return lowByte(logged, 0);
    }
    store32(out, 8, found);
    const auto formatText = load32(ptr(load32(tokens, 0)), 0x14);
    const auto format = gServices.lookupFormat != nullptr ? gServices.lookupFormat(context, formatText) : 0u;
    if (format == 0u) {
        return lowByte(format, 0);
    }
    store32(out, 0x0C, format);
    return lowByte(format, 1);
}

std::uint32_t parseEndProcCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t*, std::uint8_t*) noexcept {
    store8(context, 0x140, 0);
    return lowByte(addr(context), 1);
}

std::uint32_t parseReturnCommand(std::uint8_t*, std::uint8_t*, std::uint8_t* out, std::uint8_t*) noexcept {
    store32(out, 8, 0xFFFFFFFFu);
    return lowByte(addr(out), 1);
}

std::uint32_t parseElseCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* out, std::uint8_t* tokens) noexcept {
    if (load32(tokens, 0x0C) != 1u) {
        return lowByte(addr(tokens), 0);
    }
    auto* frame = ptr(load32(ptr(load32(context, 0x12C)), static_cast<std::ptrdiff_t>(load32(context, 0x138) * 4u - 4u)));
    const auto field = load32(frame, 0x0C);
    const auto scaled = field + field * 4u;
    const auto value = load32(context, 0x124);
    store32(ptr(load32(context, 0x118)), static_cast<std::ptrdiff_t>(scaled * 8u + 8u), value);
    frame = ptr(load32(ptr(load32(context, 0x12C)), static_cast<std::ptrdiff_t>(load32(context, 0x138) * 4u - 4u)));
    store32(frame, 0x0C, load32(context, 0x124));
    store32(out, 8, 0xFFFFFFFFu);
    store32(out, 0x0C, 4u);
    return lowByte(addr(out), 1);
}

std::uint32_t execSetClearColorCommand(std::uint8_t*, std::uint8_t*, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t*) noexcept {
    store32(state, 0x10, load32(parsed, 8));
    store32(state, 0x14, load32(parsed, 0x0C));
    store32(state, 0x18, load32(parsed, 0x10));
    store32(state, 0x1C, load32(parsed, 0x14));
    return lowByte(addr(parsed), 1);
}

std::uint32_t execSetClearDepthCommand(std::uint8_t*, std::uint8_t*, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t*) noexcept {
    store32(state, 0x20, load32(parsed, 8));
    const auto stencil = load32(parsed, 4);
    store32(state, 0x24, stencil);
    return lowByte(stencil, 1);
}

std::uint32_t execClearCommand(std::uint8_t*, std::uint8_t*, std::uint8_t*, std::uint8_t* state, std::uint8_t*) noexcept {
    return clearWithMask(state, 0x0Fu);
}

std::uint32_t execClearColorCommand(std::uint8_t*, std::uint8_t*, std::uint8_t*, std::uint8_t* state, std::uint8_t*) noexcept {
    return clearWithMask(state, 3u);
}

std::uint32_t execClearZCommand(std::uint8_t*, std::uint8_t*, std::uint8_t*, std::uint8_t* state, std::uint8_t*) noexcept {
    return clearWithMask(state, 0x0Cu);
}

std::uint32_t execClearTargetCommand(std::uint8_t*, std::uint8_t*, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t*) noexcept {
    auto* device = ptr(load32(state, 0));
    const auto called = gServices.setRenderTarget != nullptr ? gServices.setRenderTarget(device, load32(parsed, 8), 0u) : 0u;
    return lowByte(called, 1);
}

std::uint32_t execGotoCommand(std::uint8_t*, std::uint8_t*, std::uint8_t* parsed, std::uint8_t*, std::uint8_t* ip) noexcept {
    auto target = load32(parsed, 8);
    if (static_cast<std::int32_t>(target) < 0) {
        return lowByte(target, 0);
    }
    target -= 1u;
    store32(ip, 0, target);
    return lowByte(target, 1);
}

std::uint32_t execElseCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t* ip) noexcept {
    auto* frame = ptr(load32(ptr(load32(context, 0x12C)), static_cast<std::ptrdiff_t>(load32(context, 0x138) * 4u - 4u)));
    if (load32(frame, 0x0C) != 1u) {
        store32(ip, 0, load32(parsed, 8) - 1u);
    }
    return 1u;
}

std::uint32_t execElifCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t* ip) noexcept {
    auto* frame = ptr(load32(ptr(load32(context, 0x12C)), static_cast<std::ptrdiff_t>(load32(context, 0x138) * 4u - 4u)));
    if (load32(frame, 0x0C) != 1u) {
        store32(ip, 0, load32(parsed, 8) - 1u);
        return 1u;
    }
    auto* record = ptr(load32(ptr(load32(context, 0xBC)), static_cast<std::ptrdiff_t>(load32(parsed, 0x10) * 4u)));
    auto* param = ptr(load32(record, 0x0C));
    auto flag = static_cast<std::uint32_t>(load8(ptr(load32(param, 0x4C)), 0) == 0u ? 1u : 0u);
    if (load32(parsed, 0x0C) == 3u) {
        flag ^= 1u;
    }
    if (flag != 0u) {
        const auto target = load32(parsed, 8);
        if (static_cast<std::int32_t>(target) >= 0) {
            store32(ip, 0, target - 1u);
            store32(frame, 0x0C, 1u);
        }
        return 1u;
    }
    store32(frame, 0x0C, 0u);
    return 1u;
}

std::uint32_t execSetShaderOverrideCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t*) noexcept {
    const auto index = load32(parsed, 8);
    const auto tech = load32(parsed, 0x0C);
    auto* record = ptr(load32(ptr(load32(context, 8)), static_cast<std::ptrdiff_t>(index * 4u)));
    if (static_cast<std::int32_t>(tech) < 0) {
        return lowByte(addr(record), 0);
    }
    auto* shader = ptr(load32(record, 0x0C));
    if (shader == nullptr || static_cast<std::int32_t>(tech) >= static_cast<std::int32_t>(load32(record, 0x24))) {
        return lowByte(addr(record), 0);
    }
    auto* one = ptr(load32(ptr(load32(record, 0x18)), static_cast<std::ptrdiff_t>(tech * 4u)));
    const auto value = load32(one, 0x0C);
    store32(shader, 0x18, value);
    return lowByte(value, 1);
}

std::uint32_t execClearShaderOverrideCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t*) noexcept {
    auto* record = ptr(load32(ptr(load32(context, 8)), static_cast<std::ptrdiff_t>(load32(parsed, 8) * 4u)));
    auto* shader = ptr(load32(record, 0x0C));
    if (shader == nullptr) {
        return lowByte(0, 0);
    }
    store32(shader, 0x18, 0xFFFFFFFFu);
    return lowByte(addr(shader), 1);
}

std::uint32_t execSetShaderCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* out, std::uint8_t*) noexcept {
    auto* record = ptr(load32(ptr(load32(context, 8)), static_cast<std::ptrdiff_t>(load32(parsed, 8) * 4u)));
    store32(out, 4, load32(record, 0x0C));
    store32(out, 8, 0xFFFFFFFFu);
    const auto mode = load32(parsed, 0x0C);
    if (mode == 0u) {
        store32(out, 8, load32(parsed, 0x10));
    } else if (mode == 1u) {
        const auto tech = load32(parsed, 0x10);
        if (static_cast<std::int32_t>(tech) < static_cast<std::int32_t>(load32(record, 0x24))) {
            auto* one = ptr(load32(ptr(load32(record, 0x18)), static_cast<std::ptrdiff_t>(tech * 4u)));
            store32(out, 8, load32(one, 0x0C));
        }
    } else if (mode == 2u) {
        auto* item = ptr(load32(ptr(load32(context, 0xBC)), static_cast<std::ptrdiff_t>(load32(parsed, 0x10) * 4u)));
        auto* inner = ptr(load32(item, 0x0C));
        store32(out, 8, load32(ptr(load32(inner, 0x4C)), 0));
    }
    const auto called = gServices.bindShader != nullptr ? gServices.bindShader(ptr(load32(out, 0)), 0u) : 0u;
    return lowByte(called, 1);
}

std::uint32_t execSetTextureCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t*) noexcept {
    auto* scope = ptr(load32(ptr(load32(context, 0xBC)), static_cast<std::ptrdiff_t>(load32(parsed, 8) * 4u)));
    auto* texture = ptr(load32(ptr(load32(context, 0x1C)), static_cast<std::ptrdiff_t>(load32(parsed, 0x0C) * 4u)));
    auto* shader = ptr(load32(scope, 0x0C));
    const auto vtable = load32(texture, 0);
    const auto queried = slot0(vtable, 4)(texture, nullptr);
    std::uint32_t result = queried;
    if (shader != nullptr) {
        auto* current = ptr(load32(shader, 0x4C));
        if (current == nullptr || load32(current, 0) != queried) {
            auto* slot = ptr(load32(shader, 0x50));
            markParamDirty(shader, slot, queried, false);
            result = load32(shader, 0x34);
        }
    }
    return lowByte(result, 1);
}

std::uint32_t execCastTextureCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t*) noexcept {
    auto* texture = ptr(load32(ptr(load32(context, 0x1C)), static_cast<std::ptrdiff_t>(load32(parsed, 8) * 4u)));
    const auto vtable = load32(texture, 0);
    auto* surface = ptr(slot0(vtable, 4)(texture, nullptr));
    const auto surfaceVt = load32(surface, 0);
    const auto called = slot1(surfaceVt, 0x1C)(surface, nullptr, load32(parsed, 0x0C));
    return lowByte(called, 0);
}

std::uint32_t execSetIntCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t*) noexcept {
    auto* record = ptr(load32(ptr(load32(context, 0xBC)), static_cast<std::ptrdiff_t>(load32(parsed, 4) * 4u)));
    auto* param = ptr(load32(record, 0x0C));
    auto* current = ptr(load32(param, 0x4C));
    std::uint32_t result = addr(param);
    if (current == nullptr || load32(current, 0) != load32(parsed, 8)) {
        markParamDirty(param, ptr(load32(param, 0x50)), load32(parsed, 8), false);
        result = load32(param, 0x44);
    }
    return lowByte(result, 1);
}

std::uint32_t execSetBoolCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t*) noexcept {
    auto* record = ptr(load32(ptr(load32(context, 0xBC)), static_cast<std::ptrdiff_t>(load32(parsed, 4) * 4u)));
    auto* param = ptr(load32(record, 0x0C));
    auto* current = ptr(load32(param, 0x4C));
    std::uint32_t result = addr(param);
    if (current == nullptr || load8(current, 0) != load8(parsed, 8)) {
        markParamDirty(param, ptr(load32(param, 0x50)), load8(parsed, 8), true);
        result = load32(param, 0x44);
    }
    return lowByte(result, 1);
}

std::uint32_t execSetDepthCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t* state, std::uint8_t*) noexcept {
    auto index = load32(parsed, 0x0C);
    std::uint32_t surface = 0;
    if (static_cast<std::int32_t>(index) >= 0) {
        auto* record = ptr(load32(ptr(load32(context, 0x30)), static_cast<std::ptrdiff_t>(index * 4u)));
        surface = load32(record, 0x0C);
    }
    auto* device = ptr(load32(state, 0));
    const auto called = gServices.setRenderTarget != nullptr ? gServices.setRenderTarget(device, 4u, surface) : 0u;
    return lowByte(called, 1);
}

std::uint32_t execBeginEventCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t*) noexcept {
    auto* entry = ptr(load32(ptr(load32(context, 0xD0)), static_cast<std::ptrdiff_t>(load32(parsed, 0x0C) * 4u)));
    const auto vtable = load32(context, 0);
    const auto called = slot2(vtable, 0x70)(context, nullptr, load32(parsed, 8), load32(entry, 4));
    return lowByte(called, 1);
}

std::uint32_t execEndEventCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t*) noexcept {
    auto* entry = ptr(load32(ptr(load32(context, 0xD0)), static_cast<std::ptrdiff_t>(load32(parsed, 0x0C) * 4u)));
    const auto vtable = load32(context, 0);
    const auto called = slot1(vtable, 0x74)(context, nullptr, load32(entry, 4));
    return lowByte(called, 1);
}

std::uint32_t execSetMarkerCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t*) noexcept {
    auto* entry = ptr(load32(ptr(load32(context, 0xD0)), static_cast<std::ptrdiff_t>(load32(parsed, 0x0C) * 4u)));
    const auto vtable = load32(context, 0);
    const auto called = slot2(vtable, 0x78)(context, nullptr, load32(parsed, 8), load32(entry, 4));
    return lowByte(called, 1);
}

std::uint32_t execEndProcCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t*, std::uint8_t*, std::uint8_t* ip) noexcept {
    const auto depth = load32(context, 0x150);
    if (depth == 0u) {
        const auto saved = load32(context, 0x124);
        store32(ip, 0, saved);
        return lowByte(saved, 1);
    }
    const auto saved = load32(ptr(load32(context, 0x144)), static_cast<std::ptrdiff_t>(depth * 4u - 4u));
    store32(ip, 0, saved);
    auto* vec = context + 0x144;
    auto size = load32(vec, 0x0C);
    std::uint32_t result = size;
    if (static_cast<std::int32_t>(size) > 0) {
        size -= 1u;
        store32(vec, 0x0C, size);
        result = gServices.vectorShrink != nullptr ? gServices.vectorShrink(vec) : 0u;
    }
    return lowByte(result, 1);
}

std::uint32_t execCallProcCommand(std::uint8_t*, std::uint8_t* context, std::uint8_t* parsed, std::uint8_t*, std::uint8_t* ip) noexcept {
    const auto target = load32(parsed, 8);
    if (static_cast<std::int32_t>(target) < 0) {
        return lowByte(0, 1);
    }
    const auto current = load32(ip, 0);
    auto* vec = context + 0x144;
    pushVector(vec, current);
    const auto next = target - 1u;
    store32(ip, 0, next);
    const auto size = load32(vec, 0x0C);
    return lowByte(size, 1);
}

std::uint32_t clearStreamBindings(std::uint8_t* self) noexcept {
    store32(self, 0x1C, 0u);
    store32(self, 0x20, 0u);
    return 0;
}

std::uint32_t assignStreamSource(std::uint8_t* self, std::uint8_t* source, std::uint8_t* callback) noexcept {
    store32(self, 0x1C, addr(source));
    store32(self, 0x20, addr(callback));
    const auto bytes = load32(source, 0x0C) * load32(self, 0x14);
    store32(self, 0x18, bytes);
    if (callback == nullptr) {
        store32(self, 0x20, kDefaultStream);
    }
    return bytes;
}

std::uint32_t resetPassOverrides(std::uint8_t* self) noexcept {
    const auto vtable = load32(self, 0);
    slot0(vtable, 0x0C)(self, nullptr);
    store32(self, 4, 0u);
    store32(self, 8, 0u);
    store32(self, 0x0C, 0u);
    store32(self, 0x1C, 0u);
    store32(self, 0x20, 0u);
    return 0;
}

std::uint32_t applyNamedState(std::uint8_t* self, std::uint32_t name, std::uint32_t value) noexcept {
    return applyNamed(self, name, value, 0x6Cu);
}

std::uint32_t applyNamedSampler(std::uint8_t* self, std::uint32_t name, std::uint32_t value) noexcept {
    return applyNamed(self, name, value, 0x7Cu);
}

std::uint32_t applyNamedStage(std::uint8_t* self, std::uint32_t name, std::uint32_t value) noexcept {
    return applyNamed(self, name, value, 0xCCu);
}

std::uint32_t querySurfaceExtent(std::uint8_t* self, std::uint8_t* name, std::uint8_t* out) noexcept {
    auto* device = ptr(load32(self, 0x38));
    auto vtable = load32(device, 0);
    const auto found = slot2(vtable, 0x30)(ptr(vtable), nullptr, addr(device), addr(name));
    if (found == 0u) {
        return lowByte(found, 0);
    }
    device = ptr(load32(self, 0x38));
    vtable = load32(device, 0);
    std::uint32_t buffer[2] = {};
    slot3(vtable, 0x14)(device, nullptr, addr(device), found, addr(buffer));
    store32(out, 0, buffer[0]);
    store32(out, 4, buffer[1]);
    const auto sample = load8(name, static_cast<std::ptrdiff_t>(load32(self, 0x160)));
    store8(out, 8, sample);
    return lowByte(addr(out), 1);
}

std::uint32_t flushActiveTarget(std::uint8_t* self) noexcept {
    if (load32(self, 0x38) == 0u) {
        return 0;
    }
    const auto called = gServices.flushTarget != nullptr ? gServices.flushTarget(ptr(load32(self, 0x10)), addr(self + 0x48)) : 0u;
    return called;
}

std::uint32_t selectPassConstant(std::uint8_t* self, std::uint32_t index) noexcept {
    return selectConstant(self, index, load32(self, 0xB0));
}

std::uint32_t releasePrimarySurface(std::uint8_t* self) noexcept {
    return releaseSurface(self, 0x114u);
}

std::uint32_t releaseSecondarySurface(std::uint8_t* self) noexcept {
    return releaseSurface(self, 0x118u);
}

std::uint32_t bindTechniquePass(std::uint8_t* self, std::uint32_t pass, std::uint32_t index) noexcept {
    if (load32(self, 0x38) == 0u) {
        return 0;
    }
    if (pass != load32(self, 0x3C) && gServices.ensurePass != nullptr) {
        gServices.ensurePass(self, pass);
    }
    if (gServices.bindConstants != nullptr) {
        gServices.bindConstants(ptr(load32(self, 0x10)), addr(self + 0x48));
    }
    return selectConstant(self, index, load32(self + 0x48, 0x68));
}

}  // namespace ra3::render
