#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace ra3::gamelogic {

// One scripted callee result. pokeArg is the callee's stack-argument index
// whose pointer receives pokeValue, or -1 when the callee only returns.
struct ScriptQueryStub {
    std::uint32_t target = 0;
    std::uint32_t eax = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    int pokeArg = -1;
    std::uint8_t pokeValue = 0;
};

struct ScriptQueryEvent {
    std::uint32_t target = 0;
    std::uint32_t ecx = 0;
    std::uint32_t eaxIn = 0;
    std::vector<std::uint32_t> args;
};

// Flat memory and register file shared with the oracle. Addresses are the
// same numbers Unicorn uses (heap objects, TheScriptEngine global, stack).
struct ScriptQueryCpu {
    std::uint32_t eax = 0;
    std::uint32_t ecx = 0;
    std::uint32_t edx = 0;
    std::uint32_t ebx = 0;
    std::uint32_t esi = 0;
    std::uint32_t edi = 0;
    std::uint32_t esp = 0;
    std::unordered_map<std::uint32_t, std::uint8_t> mem;
    std::vector<ScriptQueryStub> stubs;
    std::size_t stubIndex = 0;
    std::vector<ScriptQueryEvent> trace;

    std::uint32_t r32(std::uint32_t addr) const;
    std::uint16_t r16(std::uint32_t addr) const;
    std::uint8_t r8(std::uint32_t addr) const;
    void w32(std::uint32_t addr, std::uint32_t value);
    void w8(std::uint32_t addr, std::uint8_t value);
    std::uint32_t stack(std::uint32_t offset) const;
    void push(std::uint32_t value);
    std::uint32_t pop();

    void lookupObject();
    void callStub(std::uint32_t target, int cleanup, int argCount = -1);
    void tailStub(std::uint32_t target, int cleanup, int argCount = -1);
    void stringCopy();
    void stringDestroy();
    void stringFromLiteral();
    void stringAppend();
    void appendDebugMessage();
    void finish();
};

std::uint32_t runScriptNamedQuery(ScriptQueryCpu& cpu, std::uint32_t va);

}  // namespace ra3::gamelogic
