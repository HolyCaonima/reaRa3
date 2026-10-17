#pragma once

#include <cstdint>
#include <unordered_map>

namespace ra3::gamelogic {

// Memory and call stand-in for the 0x007C0000 AI state methods that were still
// unanalyzed after the AIStateMachine7C batch. `call` is invoked with the
// original target, ECX and stack arguments. The callee's stack cleanup is
// applied by the caller of `call` only in the emulator; this side just
// records the arguments and the EAX the stub returns.
struct AIState7CSpan {
    std::uint32_t base = 0;
    std::uint32_t size = 0;
    std::uint8_t* data = nullptr;
};

struct AIState7CEnv {
    std::uint32_t (*call)(void* ctx, std::uint32_t target, std::uint32_t ecx,
                          const std::uint32_t* args, int argc) = nullptr;
    void* callCtx = nullptr;
    std::unordered_map<std::uint32_t, std::uint32_t> overrides;
    AIState7CSpan spans[24]{};
    int spanCount = 0;
    std::uint32_t frameAddr = 0;
    std::uint32_t frameBytes = 0;
    std::uint32_t entryEsp = 0;
};

std::uint32_t callExternal7C(AIState7CEnv& env, std::uint32_t target, std::uint32_t ecx,
                             const std::uint32_t* args, int argc);

bool runAIState7C(std::uint32_t va, std::uint8_t* self, std::uint32_t eaxIn,
                  const std::uint32_t* stack, int argc, AIState7CEnv& env, std::uint32_t& eaxOut);

}  // namespace ra3::gamelogic
