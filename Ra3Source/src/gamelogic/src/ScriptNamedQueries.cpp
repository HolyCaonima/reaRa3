// Named-object script queries anchored by the assertion at 0x00C4E740
// ("ScriptConditions::evaluateNamedUnitRankLevel: Unit not found: ").
// Generals ScriptConditions.cpp / ScriptActions.cpp supply vocabulary only;
// the bodies are the RA3 instruction sequences.
#include "ra3/gamelogic/ScriptNamedQueries.h"

#include <cstring>
#include <string>

namespace ra3::gamelogic {
namespace {

constexpr std::uint32_t kReturnSentinel = 0x7FE00FF0u;
constexpr std::uint32_t kEngine = 0x00DFC058u;
constexpr std::uint32_t kDebugOnce = 0x00DFC164u;
constexpr std::uint32_t kEmptyString = 0x00BD282Cu;
constexpr std::uint32_t kFloatGate = 0x00BD9528u;
constexpr std::uint32_t kGameLogic = 0x00DEBA64u;
constexpr std::uint32_t kLookup = 0x00837420u;
constexpr std::uint32_t kLookupEcx = 0xC0FFEE11u;
constexpr std::uint32_t kLookupEdx = 0xD0FFEE22u;
constexpr std::uint32_t kStringCopyEax = 0xA5A5A5C0u;
constexpr std::uint32_t kStringDtorEax = 0xA5A5A500u;
constexpr std::uint32_t kStringCtorEax = 0xA5A5A5B0u;
constexpr std::uint32_t kStringAppendEax = 0xA5A5A5D0u;
constexpr std::uint32_t kDebugEax = 0xA5A5A5E0u;

[[noreturn]] void fail(const std::string& text) {
    throw std::runtime_error(text);
}

std::string hex8(std::uint32_t value) {
    static const char* digits = "0123456789abcdef";
    std::string out(8, '0');
    for (int i = 7; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = digits[value & 0xF];
        value >>= 4;
    }
    return out;
}

std::uint32_t clearAl(std::uint32_t value) { return value & 0xFFFFFF00u; }

std::uint32_t withAl(std::uint32_t value, std::uint32_t al) {
    return clearAl(value) | (al & 0xFFu);
}

bool comissBelow(std::uint32_t lhsBits, std::uint32_t rhsBits) {
    float lhs = 0.0F;
    float rhs = 0.0F;
    std::memcpy(&lhs, &lhsBits, 4);
    std::memcpy(&rhs, &rhsBits, 4);
    return !(lhs >= rhs);
}

std::uint32_t cvtsi2ssBits(std::uint32_t bits) {
    const float converted = static_cast<float>(static_cast<std::int32_t>(bits));
    std::uint32_t out = 0;
    std::memcpy(&out, &converted, 4);
    return out;
}

void note(ScriptQueryCpu& cpu, std::uint32_t target, int argCount) {
    ScriptQueryEvent event;
    event.target = target;
    event.ecx = cpu.ecx;
    event.eaxIn = cpu.eax;
    for (int i = 0; i < argCount; ++i) {
        event.args.push_back(cpu.r32(cpu.esp + 4u + 4u * static_cast<std::uint32_t>(i)));
    }
    cpu.trace.push_back(event);
}

void applyStub(ScriptQueryCpu& cpu, std::uint32_t target, int cleanup, int argCount, bool tail) {
    if (!tail) {
        cpu.push(kReturnSentinel);
    }
    if (cpu.stubIndex >= cpu.stubs.size() || cpu.stubs[cpu.stubIndex].target != target) {
        const std::uint32_t got = cpu.stubIndex < cpu.stubs.size()
            ? cpu.stubs[cpu.stubIndex].target : 0;
        fail("stub target 0x" + hex8(target) + " but queue has 0x" + hex8(got));
    }
    const ScriptQueryStub item = cpu.stubs[cpu.stubIndex++];
    if (item.pokeArg >= 0) {
        const auto slot = static_cast<std::uint32_t>(item.pokeArg);
        const std::uint32_t ptr = cpu.r32(cpu.esp + 4u + 4u * slot);
        cpu.w8(ptr, item.pokeValue);
    }
    note(cpu, target, argCount < 0 ? cleanup / 4 : argCount);
    cpu.eax = item.eax;
    cpu.ecx = item.ecx;
    cpu.edx = item.edx;
    cpu.esp += 4u + static_cast<std::uint32_t>(cleanup);
}

}  // namespace

std::uint32_t ScriptQueryCpu::r32(std::uint32_t addr) const {
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        value |= static_cast<std::uint32_t>(r8(addr + static_cast<std::uint32_t>(i)))
            << (8 * i);
    }
    return value;
}

std::uint16_t ScriptQueryCpu::r16(std::uint32_t addr) const {
    return static_cast<std::uint16_t>(r8(addr) | (static_cast<std::uint16_t>(r8(addr + 1)) << 8));
}

std::uint8_t ScriptQueryCpu::r8(std::uint32_t addr) const {
    const auto it = mem.find(addr);
    if (it == mem.end()) {
        fail("unmapped read 0x" + hex8(addr));
    }
    return it->second;
}

void ScriptQueryCpu::w32(std::uint32_t addr, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        w8(addr + static_cast<std::uint32_t>(i), static_cast<std::uint8_t>(value >> (8 * i)));
    }
}

void ScriptQueryCpu::w8(std::uint32_t addr, std::uint8_t value) { mem[addr] = value; }

std::uint32_t ScriptQueryCpu::stack(std::uint32_t offset) const { return r32(esp + offset); }

void ScriptQueryCpu::push(std::uint32_t value) {
    esp -= 4;
    w32(esp, value);
}

std::uint32_t ScriptQueryCpu::pop() {
    const std::uint32_t value = r32(esp);
    esp += 4;
    return value;
}

void ScriptQueryCpu::lookupObject() {
    push(kReturnSentinel);
    const std::uint32_t param = r32(esp + 4);
    const std::uint32_t object = r32(param);
    note(*this, kLookup, 1);
    eax = object;
    ecx = kLookupEcx;
    edx = kLookupEdx;
    esp += 8;
}

void ScriptQueryCpu::callStub(std::uint32_t target, int cleanup, int argCount) {
    applyStub(*this, target, cleanup, argCount, false);
}

void ScriptQueryCpu::tailStub(std::uint32_t target, int cleanup, int argCount) {
    applyStub(*this, target, cleanup, argCount, true);
}

void ScriptQueryCpu::stringCopy() {
    push(kReturnSentinel);
    const std::uint32_t source = r32(esp + 4);
    note(*this, 0x004CA8F0u, 1);
    for (int i = 0; i < 4; ++i) {
        w8(ecx + static_cast<std::uint32_t>(i), r8(source + static_cast<std::uint32_t>(i)));
    }
    eax = kStringCopyEax;
    esp += 8;
}

void ScriptQueryCpu::stringDestroy() {
    push(kReturnSentinel);
    note(*this, 0x004CA930u, 0);
    eax = kStringDtorEax;
    esp += 4;
}

void ScriptQueryCpu::stringFromLiteral() {
    push(kReturnSentinel);
    const std::uint32_t literal = r32(esp + 4);
    note(*this, 0x004CBFF0u, 1);
    w32(ecx, literal);
    eax = kStringCtorEax;
    esp += 8;
}

void ScriptQueryCpu::stringAppend() {
    push(kReturnSentinel);
    note(*this, 0x004CB6E0u, 2);
    eax = kStringAppendEax;
    esp += 12;
}

void ScriptQueryCpu::appendDebugMessage() {
    push(kReturnSentinel);
    note(*this, 0x0081FCE0u, 2);
    eax = kDebugEax;
    esp += 12;
}

void ScriptQueryCpu::finish() {
    if (stubIndex != stubs.size()) {
        fail("unused stubs remain");
    }
}

namespace {

std::uint32_t engineOf(ScriptQueryCpu& cpu) { return cpu.r32(kEngine); }

std::uint32_t lookupParam(ScriptQueryCpu& cpu, std::uint32_t param) {
    cpu.eax = param;
    cpu.ecx = engineOf(cpu);
    cpu.push(param);
    cpu.lookupObject();
    return cpu.eax;
}

std::uint32_t evaluateNamedUnitRankLevel(ScriptQueryCpu& cpu) {
    cpu.ecx = engineOf(cpu);
    cpu.push(cpu.esi);
    cpu.esi = cpu.stack(8);
    cpu.push(cpu.esi);
    cpu.lookupObject();
    if (cpu.eax != 0) {
        const std::uint32_t tracker = cpu.r32(cpu.eax + 0x3CC);
        if (tracker == 0) {
            cpu.pop();
            cpu.eax = 0;
            return 0;
        }
        const std::uint32_t rank = cpu.r32(tracker + 0x24);
        const std::uint32_t limit = cpu.r32(cpu.stack(0xC) + 8);
        cpu.pop();
#if defined(RA3_GAMELOGIC82_ORACLE_MUTANT)
        const bool pass = static_cast<std::int32_t>(rank) < static_cast<std::int32_t>(limit);
#else
        const bool pass = static_cast<std::int32_t>(rank) >= static_cast<std::int32_t>(limit);
#endif
        cpu.eax = pass ? 1u : 0u;
        return cpu.eax;
    }
    if (cpu.r8(kDebugOnce) != 0) {
        cpu.pop();
        cpu.eax = 0;
        return 0;
    }
    cpu.push(0x00C4E740u);
    cpu.ecx = cpu.esp + 0xC;
    cpu.w8(kDebugOnce, 1);
    cpu.stringFromLiteral();
    cpu.esi += 0x10;
    cpu.push(cpu.esi);
    cpu.ecx = cpu.esp + 0x10;
    cpu.stringCopy();
    std::uint32_t buffer = cpu.stack(0xC);
    std::uint32_t length = 0;
    if (buffer == 0) {
        buffer = kEmptyString;
    } else {
        length = cpu.r16(buffer - 4);
    }
    cpu.push(length);
    cpu.push(buffer);
    cpu.eax = buffer;
    cpu.ecx = cpu.esp + 0x10;
    cpu.stringAppend();
    cpu.ecx = cpu.esp + 0xC;
    cpu.stringDestroy();
    cpu.ecx = engineOf(cpu);
    cpu.push(0);
    const std::uint32_t message = cpu.esp + 0xC;
    cpu.eax = message;
    cpu.push(message);
    cpu.appendDebugMessage();
    cpu.ecx = cpu.esp + 8;
    cpu.stringDestroy();
    cpu.eax = clearAl(cpu.eax);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedObjectPresent(ScriptQueryCpu& cpu) {
    lookupParam(cpu, cpu.stack(4));
    cpu.eax = cpu.eax == 0 ? 0u : 1u;
    return cpu.eax;
}

std::uint32_t namedStatusBit0(ScriptQueryCpu& cpu) {
    cpu.ecx = engineOf(cpu);
    cpu.push(cpu.esi);
    cpu.esi = cpu.stack(8);
    cpu.push(cpu.esi);
    cpu.lookupObject();
    if (cpu.eax != 0) {
        cpu.eax = cpu.r8(cpu.eax + 0x471) & 1u;
        cpu.pop();
        return cpu.eax;
    }
    cpu.push(cpu.ebx);
    cpu.esi += 0x10;
    cpu.push(cpu.esi);
    cpu.ecx = cpu.esp + 0x10;
    cpu.stringCopy();
    cpu.ecx = engineOf(cpu);
    cpu.eax = cpu.esp + 0xC;
    cpu.push(cpu.eax);
    cpu.callStub(0x0081BFF0u, 4);
    cpu.ebx = cpu.eax & 0xFFu;
    cpu.ecx = cpu.esp + 0xC;
    cpu.stringDestroy();
    cpu.eax = withAl(cpu.eax, cpu.ebx != 0 ? 1u : 0u);
    cpu.pop();
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedStatusBit0Clear(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        cpu.eax = 0;
        return 0;
    }
    cpu.eax = (~static_cast<std::uint32_t>(cpu.r8(object + 0x471))) & 1u;
    return cpu.eax;
}

std::uint32_t namedStatusBit0Keep(ScriptQueryCpu& cpu) {
    cpu.push(cpu.ecx);
    cpu.ecx = engineOf(cpu);
    cpu.push(cpu.esi);
    cpu.esi = cpu.stack(0xC);
    cpu.push(cpu.esi);
    cpu.lookupObject();
    if (cpu.eax != 0) {
        cpu.eax = cpu.r8(cpu.eax + 0x471) & 1u;
        cpu.pop();
        cpu.pop();
        return cpu.eax;
    }
    cpu.esi += 0x10;
    cpu.push(cpu.esi);
    cpu.ecx = cpu.esp + 8;
    cpu.stringCopy();
    cpu.ecx = engineOf(cpu);
    cpu.eax = cpu.esp + 4;
    cpu.push(cpu.eax);
    cpu.callStub(0x0081BFF0u, 4);
    cpu.ecx = cpu.esp + 4;
    cpu.stringDestroy();
    cpu.eax = clearAl(cpu.eax);
    cpu.pop();
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedFoundFalseOrDestroyed(ScriptQueryCpu& cpu) {
    cpu.ecx = engineOf(cpu);
    cpu.push(cpu.esi);
    cpu.esi = cpu.stack(8);
    cpu.push(cpu.esi);
    cpu.lookupObject();
    if (cpu.eax != 0) {
        cpu.eax = clearAl(cpu.eax);
        cpu.pop();
        return cpu.eax;
    }
    cpu.push(cpu.ebx);
    cpu.esi += 0x10;
    cpu.push(cpu.esi);
    cpu.ecx = cpu.esp + 0x10;
    cpu.stringCopy();
    cpu.ecx = engineOf(cpu);
    cpu.eax = cpu.esp + 0xC;
    cpu.push(cpu.eax);
    cpu.callStub(0x0081BFF0u, 4);
    cpu.ebx = cpu.eax & 0xFFu;
    cpu.ecx = cpu.esp + 0xC;
    cpu.stringDestroy();
    cpu.eax = withAl(cpu.eax, cpu.ebx != 0 ? 1u : 0u);
    cpu.pop();
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedKindofBit(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t param = cpu.stack(8);
    const std::uint32_t object = lookupParam(cpu, param);
    cpu.esi = object;
    if (cpu.esi == 0) {
        cpu.pop();
        cpu.eax = 0;
        return 0;
    }
    const std::uint32_t index = cpu.r32(cpu.stack(0xC) + 8);
    const std::uint32_t mask = 1u << (index & 31u);
    const std::uint32_t word = cpu.r32(cpu.esi + ((index >> 5) * 4u) + 0x80);
    cpu.pop();
    cpu.eax = (mask & word) != 0 ? 1u : 0u;
    return cpu.eax;
}

std::uint32_t namedRankAboveFrame(ScriptQueryCpu& cpu) {
    const std::uint32_t param = cpu.stack(4);
    if (param == 0) {
        cpu.eax = 0;
        return 0;
    }
    const std::uint32_t object = lookupParam(cpu, param);
    if (object == 0) {
        cpu.eax = 0;
        return 0;
    }
    const std::uint32_t tracker = cpu.r32(object + 0x3CC);
    if (tracker == 0) {
        cpu.eax = clearAl(object);
        return cpu.eax;
    }
    cpu.push(cpu.esi);
    cpu.esi = cpu.r32(tracker + 0x24);
    cpu.ecx = tracker;
    cpu.callStub(0x006867D0u, 0);
    const std::uint32_t frame = cpu.eax;
    const bool above = static_cast<std::int32_t>(cpu.esi) > static_cast<std::int32_t>(frame);
    cpu.pop();
    cpu.eax = above ? withAl(frame, 1) : clearAl(frame);
    return cpu.eax;
}

std::uint32_t namedExperienceCountEquals(ScriptQueryCpu& cpu) {
    const std::uint32_t param = cpu.stack(4);
    cpu.esp -= 8;
    cpu.push(cpu.esi);
    if (param == 0) {
        cpu.pop();
        cpu.esp += 8;
        cpu.eax = 0;
        return 0;
    }
    cpu.esi = cpu.stack(0x14);
    if (cpu.esi == 0) {
        cpu.eax = clearAl(param);
        cpu.pop();
        cpu.esp += 8;
        return cpu.eax;
    }
    const std::uint32_t object = lookupParam(cpu, param);
    if (object == 0) {
        cpu.pop();
        cpu.esp += 8;
        cpu.eax = 0;
        return 0;
    }
    const std::uint32_t expected = cpu.r32(cpu.esi + 0x2C);
    cpu.w32(cpu.esp + 4, cpu.r32(cpu.esi + 0x28));
    if (expected == 0) {
        cpu.eax = clearAl(object);
        cpu.pop();
        cpu.esp += 8;
        return cpu.eax;
    }
    const std::uint32_t tracker = cpu.r32(object + 0x3CC);
    if (tracker == 0) {
        cpu.pop();
        cpu.esp += 8;
        cpu.eax = 0;
        return 0;
    }
    const std::uint32_t actual = cpu.r32(tracker + 8);
    cpu.eax = withAl(actual, actual == expected ? 1u : 0u);
    cpu.pop();
    cpu.esp += 8;
    return cpu.eax;
}

std::uint32_t forwardModuleCall(ScriptQueryCpu& cpu, std::uint32_t offset, std::uint32_t target) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint32_t module = cpu.r32(object + offset);
    if (module == 0) {
        cpu.eax = object;
        return object;
    }
    const std::uint32_t arg = cpu.stack(8);
    cpu.push(arg);
    cpu.ecx = module;
    cpu.callStub(target, 4);
    return cpu.eax;
}

std::uint32_t drawableForward(ScriptQueryCpu& cpu, std::uint32_t target) {
    return forwardModuleCall(cpu, 0xF4, target);
}

std::uint32_t namedAiBranch(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    cpu.ecx = cpu.r32(object + 0x374);
    if (cpu.ecx == 0) {
        cpu.eax = object;
        return object;
    }
    const std::uint32_t target = cpu.r8(cpu.esp + 8) == 0 ? 0x006CCF40u : 0x006CCF10u;
    cpu.callStub(target, 0);
    return cpu.eax;
}

std::uint32_t namedClearRepulsor(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    cpu.push(cpu.esi);
    cpu.esi = cpu.r32(object + 0x374);
    if (cpu.esi == 0) {
        cpu.pop();
        cpu.eax = object;
        return object;
    }
    cpu.ecx = object;
    cpu.callStub(0x006879B0u, 0);
    cpu.push(1);
    cpu.push(0);
    cpu.ecx = cpu.esi + 0x24;
    cpu.callStub(0x006C5BE0u, 8);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedSellAndTransfer(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(8));
    cpu.esi = object;
    if (cpu.esi == 0 || cpu.r32(cpu.esi + 0x374) == 0) {
        cpu.pop();
        return cpu.eax;
    }
    cpu.push(0x184);
    cpu.ecx = cpu.esi;
    cpu.callStub(0x004D9AF0u, 4);
    cpu.ecx = cpu.esi;
    cpu.callStub(0x00686FA0u, 0);
    cpu.push(1);
    cpu.push(cpu.esi);
    cpu.ecx = cpu.eax;
    cpu.callStub(0x007E1550u, 8);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedHandToLogic(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    cpu.ecx = cpu.r32(kGameLogic);
    cpu.w32(cpu.esp + 4, object);
    cpu.tailStub(0x004EDD70u, 4);
    return cpu.eax;
}

std::uint32_t namedSetStance(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    cpu.push(0);
    cpu.push(1);
    cpu.push(6);
    cpu.ecx = object;
    cpu.callStub(0x007483A0u, 12);
    return cpu.eax;
}

std::uint32_t namedSetExperience(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    cpu.push(cpu.stack(8));
    cpu.ecx = object;
    cpu.callStub(0x00687000u, 4);
    return cpu.eax;
}

std::uint32_t namedCopyPosition(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint32_t position = object + 0x38;
    if (position == 0) {
        cpu.eax = 0;
        return 0;
    }
    cpu.ecx = cpu.stack(8);
    cpu.eax = position;
    cpu.push(cpu.ecx);
    cpu.push(position);
    cpu.callStub(0x00822890u, 0, 2);
    cpu.esp += 8;
    return cpu.eax;
}

std::uint32_t namedModuleForwardB(ScriptQueryCpu& cpu) {
    return drawableForward(cpu, 0x00524530u);
}

std::uint32_t namedRelationTail(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint8_t flag = cpu.r8(cpu.esp + 8);
    cpu.w32(cpu.esp + 4, 8);
    const std::uint32_t mixed = (cpu.ecx & 0xFFFFFF00u) | (flag == 0 ? 1u : 0u);
    cpu.w32(cpu.esp + 8, mixed);
    cpu.ecx = object;
    cpu.tailStub(0x00706890u, 8);
    return cpu.eax;
}

std::uint32_t namedFixedRelationTail(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    cpu.w32(cpu.esp + 4, 0x20);
    cpu.ecx = object;
    cpu.tailStub(0x00706890u, 8);
    return cpu.eax;
}

std::uint32_t namedProductionBitCall(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint32_t ai = cpu.r32(object + 0x374);
    if (ai == 0) {
        cpu.eax = object;
        return object;
    }
    const std::uint32_t templ = cpu.r32(object + 4);
    const std::uint32_t bits = cpu.r32(templ + 0xCC);
    cpu.eax = bits >> 10;
    cpu.push(1);
    cpu.ecx = ai + 0x24;
    cpu.push(0);
    const std::uint32_t target = ((bits >> 10) & 1u) != 0 ? 0x004DD080u : 0x004DCFF0u;
    cpu.callStub(target, 8);
    return cpu.eax;
}

std::uint32_t namedRecruitTail(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint8_t flag = cpu.r8(cpu.esp + 8);
    cpu.w32(cpu.esp + 4, 3);
    const std::uint32_t mask = flag != 0 ? 0x3FFFFFFFu : 0u;
    cpu.w32(cpu.esp + 8, mask);
    cpu.ecx = object;
    cpu.tailStub(0x00757B40u, 8);
    return cpu.eax;
}

std::uint32_t namedAiFloatFloor(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint32_t ai = cpu.r32(object + 0x374);
    if (ai == 0) {
        cpu.eax = 0;
        return 0;
    }
    const std::uint32_t body = cpu.r32(ai + 0x214);
    if (body == 0) {
        cpu.eax = 0;
        return 0;
    }
    const std::uint32_t bits = cpu.stack(8);
    if (comissBelow(bits, cpu.r32(kFloatGate))) {
        cpu.eax = body;
        return body;
    }
    cpu.w32(body + 0x34, bits);
    cpu.eax = body;
    return body;
}

std::uint32_t namedUnheldSelect(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    if ((cpu.r8(object + 0x471) & 1u) != 0) {
        cpu.eax = object;
        return object;
    }
    const std::uint32_t manager = cpu.r32(0x00DEEE5Cu);
    const std::uint32_t function = cpu.r32(cpu.r32(manager) + 0x4C);
    cpu.w32(cpu.esp + 4, object);
    cpu.ecx = manager;
    cpu.eax = function;
    cpu.tailStub(function, 4);
    return cpu.eax;
}

std::uint32_t namedWeaponOnSlot(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(8));
    cpu.esi = object;
    const std::uint32_t second = cpu.stack(0xC);
    cpu.push(second);
    cpu.ecx = cpu.r32(0x00CC569Cu);
    cpu.callStub(0x007E2950u, 4);
    if (cpu.esi == 0 || cpu.eax == 0) {
        cpu.pop();
        return cpu.eax;
    }
    const std::uint32_t player = cpu.eax;
    cpu.push(player);
    cpu.ecx = cpu.esi;
    cpu.callStub(0x00687720u, 4);
    if (cpu.eax == 0) {
        cpu.pop();
        return 0;
    }
    const std::uint32_t weapon = cpu.eax;
    const std::uint32_t arg = cpu.stack(0x10);
    const std::uint32_t function = cpu.r32(cpu.r32(weapon) + 0x28);
    cpu.push(arg);
    cpu.ecx = weapon;
    cpu.callStub(function, 4);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedAiOneTail(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint32_t ai = cpu.r32(object + 0x374);
    if (ai == 0) {
        cpu.eax = 0;
        return 0;
    }
    cpu.w32(cpu.esp + 4, 1);
    cpu.eax = ai;
    cpu.ecx = ai + 0x24;
    cpu.tailStub(0x0054F490u, 4);
    return cpu.eax;
}

std::uint32_t namedPassTwoInts(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(8));
    cpu.esi = object;
    if (cpu.esi == 0) {
        cpu.pop();
        return cpu.eax;
    }
    const std::uint32_t second = cpu.stack(0x10);
    const std::uint32_t first = cpu.stack(0xC);
    cpu.eax = first;
    cpu.ecx = second;
    cpu.push(second);
    cpu.callStub(0x0081B730u, 0, 1);
    cpu.esp += 4;
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedClassifyText(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(8));
    cpu.esi = object;
    if (cpu.esi == 0) {
        cpu.pop();
        return cpu.eax;
    }
    const std::uint32_t spec = cpu.stack(0xC);
    std::uint32_t text = cpu.r32(spec);
    const std::uint32_t flagSlot = cpu.esp + 8;
    cpu.w8(flagSlot, 0);
    if (text == 0) {
        text = kEmptyString;
    }
    cpu.eax = text;
    cpu.ecx = spec;
    cpu.push(flagSlot);
    cpu.push(text);
    cpu.callStub(0x00401B40u, 0, 2);
    cpu.esp += 8;
    if (cpu.r8(flagSlot) == 0) {
        cpu.pop();
        return cpu.eax;
    }
    cpu.push(cpu.eax);
    cpu.ecx = cpu.esi;
    cpu.callStub(0x004D9AB0u, 4);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedLinkPair(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    cpu.ecx = cpu.stack(8);
    cpu.push(cpu.ecx);
    cpu.push(object);
    cpu.callStub(0x00803800u, 0, 2);
    cpu.esp += 8;
    return cpu.eax;
}

std::uint32_t namedEnableProduced(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    cpu.push(cpu.esi);
    cpu.ecx = object;
    cpu.callStub(0x006A7600u, 0);
    cpu.esi = cpu.eax;
    if (cpu.esi == 0) {
        cpu.pop();
        return cpu.eax;
    }
    const std::uint32_t first = cpu.r32(cpu.r32(cpu.esi) + 0x10C);
    cpu.eax = first;
    cpu.push(0);
    cpu.ecx = cpu.esi;
    cpu.callStub(first, 4);
    if (cpu.eax == 0) {
        cpu.pop();
        return 0;
    }
    const std::uint32_t second = cpu.r32(cpu.r32(cpu.esi) + 0x108);
    cpu.eax = second;
    cpu.push(1);
    cpu.push(0);
    cpu.push(0x008038E0u);
    cpu.ecx = cpu.esi;
    cpu.callStub(second, 12);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedAttackIfFree(ScriptQueryCpu& cpu, std::uint32_t finalTarget, int finalCleanup) {
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(0xC));
    cpu.esi = object;
    if (cpu.esi == 0) {
        cpu.pop();
        return cpu.eax;
    }
    cpu.push(cpu.stack(8));
    cpu.ecx = cpu.r32(0x00DEEE80u);
    cpu.callStub(0x00549BA0u, 4);
    if (cpu.eax == 0) {
        cpu.pop();
        return 0;
    }
    const std::uint32_t templ = cpu.r32(cpu.esi + 4);
    const std::uint32_t bits = cpu.r32(templ + 0xCC);
    if (((bits >> 10) & 1u) != 0) {
        cpu.pop();
        return cpu.eax;
    }
    const std::uint32_t target = cpu.eax;
    if (finalCleanup == 8) {
        cpu.push(0);
    }
    cpu.push(target);
    cpu.ecx = cpu.esi;
    cpu.callStub(finalTarget, finalCleanup);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedExperienceScaled(ScriptQueryCpu& cpu, std::uint32_t target, int cleanup,
    bool fourFlags) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint32_t tracker = cpu.r32(object + 0x3CC);
    if (tracker == 0) {
        cpu.eax = object;
        return object;
    }
    const std::uint32_t bits = cvtsi2ssBits(cpu.stack(8));
    if (fourFlags) {
        cpu.push(0);
        cpu.push(1);
        cpu.push(1);
        cpu.push(1);
    } else {
        cpu.push(1);
    }
    cpu.push(tracker);
    cpu.w32(cpu.esp, bits);
    cpu.ecx = tracker;
    cpu.callStub(target, cleanup);
    return cpu.eax;
}

std::uint32_t namedWriteRank(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint32_t tracker = cpu.r32(object + 0x3CC);
    if (tracker == 0) {
        cpu.eax = 0;
        return 0;
    }
    cpu.w32(tracker + 0x28, cpu.stack(8));
    cpu.eax = tracker;
    return tracker;
}

std::uint32_t namedForwardOnThis(ScriptQueryCpu& cpu) {
    const std::uint32_t savedThis = cpu.ecx;
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(8));
    if (object == 0) {
        cpu.pop();
        return 0;
    }
    const std::uint32_t third = cpu.stack(0x10);
    const std::uint32_t second = cpu.stack(0xC);
    cpu.push(third);
    cpu.push(second);
    cpu.push(object);
    cpu.ecx = savedThis;
    cpu.callStub(0x0081F0D0u, 12);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedFlaggedCommand(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint32_t spec = cpu.stack(8);
    const std::uint32_t value = cpu.r32(spec + 8);
    cpu.w32(cpu.esp + 4, 0x47);
    const std::uint32_t mixed = (cpu.edx & 0xFFFFFF00u) | (value != 0 ? 1u : 0u);
    cpu.ecx = object;
    cpu.w32(cpu.esp + 8, mixed);
    cpu.tailStub(0x006D2020u, 8);
    return cpu.eax;
}

std::uint32_t namedAttachUpgrade(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(8));
    cpu.esi = object;
    if (cpu.esi == 0) {
        cpu.pop();
        return cpu.eax;
    }
    cpu.push(cpu.stack(0xC));
    cpu.ecx = cpu.r32(0x00DFC190u);
    cpu.callStub(0x00861A70u, 4);
    if (cpu.eax == 0) {
        cpu.pop();
        return 0;
    }
    const std::uint32_t upgrade = cpu.eax + 0xC;
    cpu.eax = upgrade;
    cpu.push(0);
    cpu.push(upgrade);
    cpu.ecx = cpu.esi;
    cpu.callStub(0x006F0910u, 8);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedExperienceArg(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint32_t tracker = cpu.r32(object + 0x3CC);
    if (tracker == 0) {
        cpu.eax = object;
        return object;
    }
    cpu.push(cpu.stack(8));
    cpu.ecx = tracker;
    cpu.callStub(0x006EE7B0u, 4);
    return cpu.eax;
}

std::uint32_t namedTwoArgCommand(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint32_t second = cpu.stack(0xC);
    const std::uint32_t first = cpu.stack(8);
    cpu.push(second);
    cpu.push(first);
    cpu.ecx = object;
    cpu.callStub(0x006D2020u, 8);
    return cpu.eax;
}

std::uint32_t namedBindPair(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t first = lookupParam(cpu, cpu.stack(8));
    cpu.esi = first;
    if (cpu.esi == 0) {
        cpu.pop();
        return cpu.eax;
    }
    cpu.push(cpu.stack(0xC));
    cpu.ecx = engineOf(cpu);
    cpu.lookupObject();
    if (cpu.eax == 0) {
        cpu.pop();
        cpu.eax = 0;
        return 0;
    }
    cpu.push(cpu.edi);
    cpu.edi = cpu.eax;
    cpu.eax = cpu.esi;
    cpu.callStub(0x0081F6C0u, 0, 0);
    cpu.pop();
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedVirtualAttach(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    cpu.eax = cpu.stack(8);
    cpu.push(cpu.eax);
    cpu.callStub(0x004E6D30u, 0, 1);
    cpu.esp += 4;
    cpu.esi = cpu.eax;
    if (cpu.esi == 0) {
        cpu.pop();
        return cpu.eax;
    }
    cpu.push(cpu.stack(0xC));
    cpu.ecx = engineOf(cpu);
    cpu.lookupObject();
    if (cpu.eax == 0) {
        cpu.pop();
        return cpu.eax;
    }
    const std::uint8_t flag = cpu.r8(cpu.esp + 0x10);
    const std::uint32_t function = cpu.r32(cpu.r32(cpu.esi) + (flag != 0 ? 0x34u : 0x2Cu));
    const std::uint32_t object = cpu.eax;
    cpu.eax = function;
    cpu.push(object);
    cpu.ecx = cpu.esi;
    cpu.callStub(function, 4);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedPlayOrStop(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(8));
    cpu.esi = object;
    if (cpu.esi == 0) {
        cpu.pop();
        return cpu.eax;
    }
    std::uint32_t text = cpu.r32(cpu.stack(0xC));
    if (text == 0) {
        text = kEmptyString;
    }
    cpu.eax = text;
    cpu.ecx = cpu.stack(0xC);
    cpu.push(0);
    cpu.push(text);
    cpu.callStub(0x00401AD0u, 0, 2);
    cpu.esp += 8;
    const std::uint8_t flag = cpu.r8(cpu.esp + 0x10);
    cpu.push(cpu.eax);
    cpu.ecx = cpu.esi;
    cpu.callStub(flag != 0 ? 0x0076C400u : 0x006F0A20u, 4);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedPublishDrawable(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        return 0;
    }
    const std::uint32_t drawable = cpu.r32(object + 0xF4);
    if (drawable == 0) {
        cpu.eax = 0;
        return 0;
    }
    cpu.eax = drawable;
    cpu.push(drawable);
    cpu.callStub(0x008230C0u, 0, 1);
    cpu.esp += 4;
    return cpu.eax;
}

std::uint32_t namedGuardBoth(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t first = lookupParam(cpu, cpu.stack(8));
    cpu.esi = first;
    cpu.push(cpu.stack(0xC));
    cpu.ecx = engineOf(cpu);
    cpu.lookupObject();
    if (cpu.esi == 0 || cpu.eax == 0) {
        cpu.pop();
        return cpu.eax;
    }
    const std::uint32_t ai = cpu.r32(cpu.esi + 0x374);
    if (ai == 0) {
        cpu.pop();
        return cpu.eax;
    }
    cpu.push(1);
    cpu.push(0x7FFFFFFFu);
    cpu.push(cpu.eax);
    cpu.ecx = ai + 0x24;
    cpu.callStub(0x006C5710u, 12);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedGuardUpgrade(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(8));
    cpu.esi = object;
    if (cpu.esi == 0) {
        cpu.pop();
        return cpu.eax;
    }
    cpu.push(cpu.stack(0xC));
    cpu.ecx = cpu.r32(0x00DFC190u);
    cpu.callStub(0x00861A70u, 4);
    if (cpu.eax == 0) {
        cpu.pop();
        return 0;
    }
    const std::uint32_t ai = cpu.r32(cpu.esi + 0x374);
    if (ai == 0) {
        cpu.pop();
        return cpu.eax;
    }
    const std::uint32_t upgrade = cpu.eax + 0xC;
    cpu.eax = upgrade;
    cpu.push(1);
    cpu.push(0x7FFFFFFFu);
    cpu.push(upgrade);
    cpu.ecx = ai + 0x24;
    cpu.callStub(0x006C57A0u, 12);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedTeamMemberBit(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(8));
    cpu.esi = object;
    if (cpu.esi == 0) {
        cpu.pop();
        cpu.eax = 0;
        return 0;
    }
    const std::uint32_t templ = cpu.r32(cpu.esi + 4);
    if (((cpu.r32(templ + 0xC8) >> 0x19) & 1u) != 0) {
        cpu.eax = clearAl(cpu.esi);
        cpu.pop();
        return cpu.eax;
    }
    const std::uint32_t spec = cpu.stack(0xC) + 0x10;
    cpu.push(0);
    cpu.ecx = cpu.esp;
    cpu.eax = spec;
    cpu.push(spec);
    cpu.stringCopy();
    cpu.ecx = engineOf(cpu);
    cpu.callStub(0x0082BA30u, 4);
    if (cpu.eax == 0) {
        cpu.pop();
        cpu.eax = 0;
        return 0;
    }
    cpu.push(cpu.eax);
    cpu.ecx = cpu.esi;
    cpu.callStub(0x006A5AB0u, 4);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedTeamMemberOther(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(8));
    cpu.esi = object;
    if (cpu.esi == 0) {
        cpu.pop();
        cpu.eax = 0;
        return 0;
    }
    const std::uint32_t spec = cpu.stack(0xC) + 0x10;
    cpu.push(0);
    cpu.ecx = cpu.esp;
    cpu.push(spec);
    cpu.stringCopy();
    cpu.ecx = engineOf(cpu);
    cpu.callStub(0x0082BA30u, 4);
    if (cpu.eax == 0) {
        cpu.pop();
        cpu.eax = 0;
        return 0;
    }
    cpu.push(cpu.eax);
    cpu.ecx = cpu.esi;
    cpu.callStub(0x006A5B10u, 4);
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedContainCompare(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(8));
    cpu.esi = object;
    if (cpu.esi == 0) {
        cpu.pop();
        cpu.eax = 0;
        return 0;
    }
    cpu.push(cpu.stack(0xC));
    cpu.ecx = engineOf(cpu);
    cpu.lookupObject();
    if (cpu.eax == 0) {
        cpu.pop();
        cpu.eax = 0;
        return 0;
    }
    const std::uint32_t secondPos = cpu.eax + 0x38;
    cpu.push(0);
    cpu.push(0);
    cpu.push(0);
    cpu.push(secondPos);
    cpu.push(cpu.esi + 0x38);
    cpu.push(cpu.esi);
    cpu.eax = secondPos;
    cpu.callStub(0x00658A80u, 0, 6);
    cpu.esp += 0x18;
    cpu.pop();
    return cpu.eax;
}

std::uint32_t namedProducedCount(ScriptQueryCpu& cpu) {
    const std::uint32_t object = lookupParam(cpu, cpu.stack(4));
    if (object == 0) {
        cpu.eax = 0;
        return 0;
    }
    cpu.ecx = object;
    cpu.callStub(0x006A7600u, 0);
    if (cpu.eax == 0) {
        cpu.eax = 0;
        return 0;
    }
    const std::uint32_t produced = cpu.eax;
    const std::uint32_t function = cpu.r32(cpu.r32(produced) + 0x10C);
    cpu.eax = function;
    cpu.push(0);
    cpu.ecx = produced;
    cpu.callStub(function, 4);
    const std::uint32_t value = cpu.eax;
    cpu.eax = (value == 0) ? 1u : clearAl(value);
    return cpu.eax;
}

std::uint32_t namedProducedPositive(ScriptQueryCpu& cpu) {
    cpu.push(cpu.esi);
    const std::uint32_t object = lookupParam(cpu, cpu.stack(8));
    cpu.esi = object;
    if (cpu.esi == 0) {
        cpu.pop();
        cpu.eax = 0;
        return 0;
    }
    cpu.ecx = cpu.esi;
    cpu.callStub(0x006A7600u, 0);
    if (cpu.eax == 0) {
        cpu.pop();
        cpu.eax = 0;
        return 0;
    }
    cpu.ecx = cpu.esi;
    cpu.callStub(0x006A7600u, 0);
    const std::uint32_t produced = cpu.eax;
    const std::uint32_t function = cpu.r32(cpu.r32(produced) + 0x10C);
    cpu.eax = function;
    cpu.push(0);
    cpu.ecx = produced;
    cpu.callStub(function, 4);
    if (cpu.eax == 0) {
        cpu.pop();
        cpu.eax = 0;
        return 0;
    }
    cpu.eax = withAl(cpu.eax, 1);
    cpu.pop();
    return cpu.eax;
}

}  // namespace

std::uint32_t runScriptNamedQuery(ScriptQueryCpu& cpu, std::uint32_t va) {
    std::uint32_t result = 0;
    switch (va) {
    case 0x00838D80u: result = drawableForward(cpu, 0x0052DB30u); break;
    case 0x00839EA0u: result = namedAiBranch(cpu); break;
    case 0x0083A570u: result = namedClearRepulsor(cpu); break;
    case 0x0083A990u: result = namedSellAndTransfer(cpu); break;
    case 0x0083ABD0u: result = namedHandToLogic(cpu); break;
    case 0x0083AC60u: result = namedSetStance(cpu); break;
    case 0x0083AC90u: result = namedSetExperience(cpu); break;
    case 0x0083AFC0u: result = namedCopyPosition(cpu); break;
    case 0x0083B260u: result = namedModuleForwardB(cpu); break;
    case 0x0083B290u: result = namedRelationTail(cpu); break;
    case 0x0083B2D0u: result = namedFixedRelationTail(cpu); break;
    case 0x0083B300u: result = namedProductionBitCall(cpu); break;
    case 0x0083B350u: result = namedRecruitTail(cpu); break;
    case 0x0083B390u: result = namedAiFloatFloor(cpu); break;
    case 0x0083B3D0u: result = namedUnheldSelect(cpu); break;
    case 0x0083B410u: result = namedWeaponOnSlot(cpu); break;
    case 0x0083C430u: result = namedAiOneTail(cpu); break;
    case 0x0083C640u: result = namedPassTwoInts(cpu); break;
    case 0x0083C8D0u: result = namedClassifyText(cpu); break;
    case 0x0083C920u: result = namedLinkPair(cpu); break;
    case 0x0083C950u: result = namedEnableProduced(cpu); break;
    case 0x0083CB30u: result = namedAttackIfFree(cpu, 0x0071C890u, 8); break;
    case 0x0083CB80u: result = namedAttackIfFree(cpu, 0x007070A0u, 4); break;
    case 0x0083CC30u: result = namedExperienceScaled(cpu, 0x006EE8F0u, 0x14, true); break;
    case 0x0083CC70u: result = namedExperienceScaled(cpu, 0x006EE9E0u, 8, false); break;
    case 0x0083CCB0u: result = namedWriteRank(cpu); break;
    case 0x0083CDD0u: result = namedForwardOnThis(cpu); break;
    case 0x0083DA80u: result = namedFlaggedCommand(cpu); break;
    case 0x0083DAC0u: result = namedAttachUpgrade(cpu); break;
    case 0x0083DC70u: result = namedExperienceArg(cpu); break;
    case 0x0083DD90u: result = namedTwoArgCommand(cpu); break;
    case 0x0083DDC0u: result = namedBindPair(cpu); break;
    case 0x0083E220u: result = namedVirtualAttach(cpu); break;
    case 0x0083E270u: result = namedPlayOrStop(cpu); break;
    case 0x0083E350u: result = namedPublishDrawable(cpu); break;
    case 0x0083E380u: result = namedGuardBoth(cpu); break;
    case 0x0083E3D0u: result = namedGuardUpgrade(cpu); break;
    case 0x0083E520u: result = evaluateNamedUnitRankLevel(cpu); break;
    case 0x0083E5D0u: result = namedStatusBit0(cpu); break;
    case 0x0083E630u: result = namedStatusBit0Clear(cpu); break;
    case 0x0083E660u: result = namedStatusBit0Keep(cpu); break;
    case 0x0083E6C0u: result = namedFoundFalseOrDestroyed(cpu); break;
    case 0x0083EBA0u: result = namedObjectPresent(cpu); break;
    case 0x0083ED80u: result = namedProducedCount(cpu); break;
    case 0x0083F6A0u: result = namedTeamMemberBit(cpu); break;
    case 0x0083F700u: result = namedTeamMemberOther(cpu); break;
    case 0x0083F7E0u: result = namedKindofBit(cpu); break;
    case 0x0083FA40u: result = namedRankAboveFrame(cpu); break;
    case 0x0083FA80u: result = namedExperienceCountEquals(cpu); break;
    case 0x0083FDB0u: result = namedContainCompare(cpu); break;
    case 0x0083FF90u: result = namedProducedPositive(cpu); break;
    default:
        fail("unknown query 0x" + hex8(va));
    }
    cpu.finish();
    return result;
}

}  // namespace ra3::gamelogic
